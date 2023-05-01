/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLVkMeshCompiler.h"
#include "XLGlMesh.h"
#include "XLVkBuffer.h"
#include "XLVkAllocator.h"
#include "XLVkTransferQueue.h"

namespace stappler::xenolith::vk {

class MeshCompilerAttachment : public renderqueue::GenericAttachment {
public:
	virtual ~MeshCompilerAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class MeshCompilerAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~MeshCompilerAttachmentHandle();

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual const Rc<gl::MeshInputData> &getInputData() const { return _inputData; }
	virtual const Rc<gl::MeshSet> &getMeshSet() const { return _originSet; }

protected:
	Rc<gl::MeshInputData> _inputData;
	Rc<gl::MeshSet> _originSet;
};

class MeshCompilerPass : public QueuePass {
public:
	virtual ~MeshCompilerPass();

	virtual bool init(StringView);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const MeshCompilerAttachment *getMeshAttachment() const {
		return _meshAttachment;
	}

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const MeshCompilerAttachment *_meshAttachment = nullptr;
};

class MeshCompilerPassHandle : public QueuePassHandle {
public:
	virtual ~MeshCompilerPassHandle();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful) override;

	virtual QueueOperations getQueueOps() const override;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;
	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

	bool loadPersistent(gl::MeshIndex *);

	Rc<gl::MeshSet> _outputData;
	MeshCompilerAttachmentHandle *_meshAttachment;
};

MeshCompiler::~MeshCompiler() { }

bool MeshCompiler::init() {
	Queue::Builder builder("MeshCompiler");

	auto attachment = Rc<MeshCompilerAttachment>::create("MeshAttachment");
	auto pass = Rc<MeshCompilerPass>::create("MeshPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addInput(attachment);
	builder.addOutput(attachment);

	if (renderqueue::Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

bool MeshCompiler::inProgress(const gl::MeshAttachment *a) const {
	auto it = _inProgress.find(a);
	if (it != _inProgress.end()) {
		return true;
	}
	return false;
}

void MeshCompiler::setInProgress(const gl::MeshAttachment *a) {
	_inProgress.emplace(a);
}

void MeshCompiler::dropInProgress(const gl::MeshAttachment *a) {
	_inProgress.erase(a);
}

bool MeshCompiler::hasRequest(const gl::MeshAttachment *a) const {
	auto it = _requests.find(a);
	if (it != _requests.end()) {
		return true;
	}
	return false;
}

void MeshCompiler::appendRequest(const gl::MeshAttachment *a, Rc<gl::MeshInputData> &&req,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) {
	auto it = _requests.find(a);
	if (it == _requests.end()) {
		it = _requests.emplace(a, MeshRequest()).first;
	}

	for (auto &rem : req->meshesToRemove) {
		auto m = it->second.toAdd.find(rem);
		if (m != it->second.toAdd.end()) {
			it->second.toAdd.erase(m);
		}

		it->second.toRemove.emplace(rem);
	}

	for (auto &m : req->meshesToAdd) {
		auto iit = it->second.toAdd.find(m);
		if (iit == it->second.toAdd.end()) {
			it->second.toAdd.emplace(m);
		}
		auto v = it->second.toRemove.find(m);
		if (v != it->second.toRemove.end()) {
			it->second.toRemove.erase(v);
		}
	}

	if (it->second.deps.empty()) {
		it->second.deps = move(deps);
	} else {
		for (auto &iit : deps) {
			it->second.deps.emplace_back(move(iit));
		}
	}
}

void MeshCompiler::clearRequests() {
	_requests.clear();
}

auto MeshCompiler::makeRequest(Rc<gl::MeshInputData> &&input,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) -> Rc<FrameRequest> {
	auto req = Rc<FrameRequest>::create(this);
	req->addInput(_attachment, move(input));
	req->addSignalDependencies(move(deps));
	return req;
}

void MeshCompiler::runMeshCompilationFrame(gl::Loop &loop, Rc<gl::MeshInputData> &&req,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) {
	auto targetAttachment = req->attachment;

	auto h = loop.makeFrame(makeRequest(move(req), move(deps)), 0);
	h->setCompleteCallback([this, targetAttachment] (FrameHandle &handle) {
		auto reqIt = _requests.find(targetAttachment);
		if (reqIt != _requests.end()) {
			if (handle.getLoop()->isRunning()) {
				auto deps = move(reqIt->second.deps);
				Rc<gl::MeshInputData> req = Rc<gl::MeshInputData>::alloc();
				req->attachment = targetAttachment;
				req->meshesToAdd.reserve(reqIt->second.toAdd.size());
				for (auto &m : reqIt->second.toAdd) {
					req->meshesToAdd.emplace_back(m);
				}
				req->meshesToRemove.reserve(reqIt->second.toRemove.size());
				for (auto &m : reqIt->second.toRemove) {
					req->meshesToRemove.emplace_back(m);
				}
				_requests.erase(reqIt);

				runMeshCompilationFrame(*handle.getLoop(), move(req), move(deps));
			} else {
				clearRequests();
				dropInProgress(targetAttachment);
			}
		} else {
			dropInProgress(targetAttachment);
		}
	});
	h->update(true);
}

MeshCompilerAttachment::~MeshCompilerAttachment() { }

auto MeshCompilerAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MeshCompilerAttachmentHandle>::create(this, handle);
}

MeshCompilerAttachmentHandle::~MeshCompilerAttachmentHandle() { }

bool MeshCompilerAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	return true;
}

void MeshCompilerAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::MeshInputData>();
	if (!d || q.isFinalized()) {
		cb(false);
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) {
		handle.performOnGlThread([this, d = move(d), cb = move(cb)] (FrameHandle &handle) {
			_inputData = d;
			_originSet = _inputData->attachment->getMeshes();
			cb(true);
		}, this, true, "MeshCompilerAttachmentHandle::submitInput");
	});
}

MeshCompilerPass::~MeshCompilerPass() { }

bool MeshCompilerPass::init(StringView name) {
	if (QueuePass::init(name, gl::RenderPassType::Generic, renderqueue::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

auto MeshCompilerPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MeshCompilerPassHandle>::create(*this, handle);
}

void MeshCompilerPass::prepare(gl::Device &) {
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<MeshCompilerAttachment *>(it->getAttachment())) {
			_meshAttachment = a;
		}
	}
}

MeshCompilerPassHandle::~MeshCompilerPassHandle() { }

bool MeshCompilerPassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(((MeshCompilerPass *)_renderPass.get())->getMeshAttachment())) {
		_meshAttachment = (MeshCompilerAttachmentHandle *)a->handle.get();
	}

	return QueuePassHandle::prepare(frame, move(cb));
}

void MeshCompilerPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);
}

QueueOperations MeshCompilerPassHandle::getQueueOps() const {
	return QueuePassHandle::getQueueOps();
}

Vector<const CommandBuffer *> MeshCompilerPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto &allocator = _device->getAllocator();
	auto memPool = static_cast<DeviceFrameHandle &>(handle).getMemPool(this);

	auto input = _meshAttachment->getInputData();
	auto prev = _meshAttachment->getMeshSet();

	QueueOperations ops = QueueOperations::None;
	for (auto &it : input->attachment->getRenderPasses()) {
		ops |= ((QueuePass *)it->renderPass.get())->getQueueOps();
	}

	auto q = _device->getQueueFamily(ops);
	if (!q) {
		return Vector<const CommandBuffer *>();
	}

	auto indexes = _meshAttachment->getMeshSet()->getIndexes();

	do {
		auto it = indexes.begin();
		while (it != indexes.end()) {
			auto iit = std::find(input->meshesToAdd.begin(), input->meshesToAdd.end(), it->index);
			if (iit != input->meshesToAdd.end()) {
				input->meshesToAdd.erase(iit);
			}

			iit = std::find(input->meshesToRemove.begin(), input->meshesToRemove.end(), it->index);
			if (iit != input->meshesToRemove.end()) {
				it = indexes.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	for (auto &it : input->meshesToAdd) {
		indexes.emplace_back(gl::MeshSet::Index{maxOf<VkDeviceSize>(), maxOf<VkDeviceSize>(), it});
	}

	uint64_t indexBufferSize = 0;
	uint64_t vertexBufferSize = 0;

	for (auto &it : indexes) {
		indexBufferSize += it.index->getIndexBufferData()->size;
		vertexBufferSize += it.index->getVertexBufferData()->size;
	}

	gl::BufferInfo vertexBufferInfo;
	gl::BufferInfo indexBufferInfo;

	if (prev) {
		vertexBufferInfo = prev->getVertexBuffer()->getInfo();
		indexBufferInfo = prev->getIndexBuffer()->getInfo();
	} else {
		vertexBufferInfo = *indexes.front().index->getVertexBufferData();
		indexBufferInfo = *indexes.front().index->getIndexBufferData();
	}

	vertexBufferInfo.size = vertexBufferSize;
	indexBufferInfo.size = indexBufferSize;

	auto vertexBuffer = allocator->spawnPersistent(AllocationUsage::DeviceLocal, vertexBufferInfo);
	auto indexBuffer = allocator->spawnPersistent(AllocationUsage::DeviceLocal, indexBufferInfo);

	auto loadBuffer = [] (const gl::BufferData *bufferData, DeviceBuffer *buf) {
		if (!bufferData->data.empty()) {
			buf->setData(bufferData->data);
		} else if (bufferData->callback) {
			auto region = buf->map(VkDeviceSize(0), maxOf<VkDeviceSize>(), false);
			bufferData->callback(region.ptr, region.size, [&] (BytesView data) {
				buf->unmap(region, false);
				region.ptr = nullptr;
				buf->setData(data);
			});
			if (region.ptr) {
				buf->unmap(region, true);
			}
		}
		return buf;
	};

	auto writeBufferCopy = [&memPool, &loadBuffer] (CommandBuffer &buf, const gl::BufferData *bufferData, Buffer *targetBuffer,
			VkDeviceSize targetOffset, VkDeviceSize originOffset, Buffer *originBuffer) -> VkDeviceSize {
		Buffer *sourceBuffer = nullptr;
		VkDeviceSize sourceOffset = 0;
		VkDeviceSize targetSize = bufferData->size;

		if (originBuffer && originOffset != maxOf<VkDeviceSize>()) {
			sourceBuffer = originBuffer;
			sourceOffset = originOffset;
		} else {
			auto resourceBuffer = bufferData->buffer.get();
			if (!resourceBuffer) {
				auto tmp = memPool->spawn(AllocationUsage::HostTransitionSource, bufferData);
				resourceBuffer = loadBuffer(bufferData, tmp);
			}

			if (resourceBuffer) {
				sourceBuffer = (Buffer *)resourceBuffer;
				sourceOffset = 0;
			}
		}

		if (sourceBuffer) {
			buf.cmdCopyBuffer(sourceBuffer, targetBuffer, sourceOffset, targetOffset, targetSize);
			return targetSize;
		}
		return 0;
	};

	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		uint64_t targetIndexOffset = 0;
		uint64_t targetVertexOffset = 0;

		Buffer *prevIndexBuffer = nullptr;
		Buffer *prevVertexBuffer = nullptr;

		if (_pool->getFamilyIdx() == q->index) {
			prevIndexBuffer = (Buffer *)prev->getIndexBuffer().get();
			prevVertexBuffer = (Buffer *)prev->getVertexBuffer().get();
		}

		for (auto &it : indexes) {
			if (_pool->getFamilyIdx() != q->index) {
				if (!loadPersistent(it.index)) {
					continue;
				}
			}

			auto indexBufferSize = writeBufferCopy(buf, it.index->getIndexBufferData(), indexBuffer,
					targetIndexOffset, it.indexOffset, prevIndexBuffer);
			if (indexBufferSize > 0) {
				it.indexOffset = targetIndexOffset;
				targetIndexOffset += indexBufferSize;
			} else {
				it.indexOffset = maxOf<VkDeviceSize>();
			}

			auto vertexBufferSize = writeBufferCopy(buf, it.index->getVertexBufferData(), vertexBuffer,
					targetVertexOffset, it.vertexOffset, prevVertexBuffer);
			if (vertexBufferSize > 0) {
				it.vertexOffset = targetIndexOffset;
				targetIndexOffset += vertexBufferSize;
			} else {
				it.vertexOffset = maxOf<VkDeviceSize>();
			}
		}
		return true;
	});

	if (buf) {
		_outputData = Rc<gl::MeshSet>::create(move(indexes), indexBuffer, vertexBuffer);

		return Vector<const CommandBuffer *>{buf};
	}
	return Vector<const CommandBuffer *>();
}

void MeshCompilerPassHandle::doSubmitted(FrameHandle &frame, Function<void(bool)> &&func, bool success) {
	if (success) {
		_meshAttachment->getInputData()->attachment->setMeshes(move(_outputData));
	}

	QueuePassHandle::doSubmitted(frame, move(func), success);
	frame.signalDependencies(success);
}

void MeshCompilerPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	QueuePassHandle::doComplete(queue, move(func), success);
}

bool MeshCompilerPassHandle::loadPersistent(gl::MeshIndex *index) {
	if (!index->isCompiled()) {
		auto res = Rc<TransferResource>::create(_device->getAllocator(), index);
		if (res->initialize(AllocationUsage::HostTransitionSource) && res->compile()) {
			return true;
		}
		return false;
	}
	return false;
}

}
