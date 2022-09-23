/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkMaterialCompiler.h"
#include "XLVkObject.h"
#include "XLVkBuffer.h"

namespace stappler::xenolith::vk {

class MaterialCompilationAttachment : public renderqueue::GenericAttachment {
public:
	virtual ~MaterialCompilationAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class MaterialCompilationAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~MaterialCompilationAttachmentHandle();

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual const Rc<gl::MaterialInputData> &getInputData() const { return _inputData; }
	virtual const Rc<gl::MaterialSet> &getOriginalSet() const { return _originalSet; }

protected:
	Rc<gl::MaterialInputData> _inputData;
	Rc<gl::MaterialSet> _originalSet;
};

class MaterialCompilationRenderPass : public QueuePass {
public:
	virtual ~MaterialCompilationRenderPass();

	virtual bool init(StringView);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const MaterialCompilationAttachment *getMaterialAttachment() const {
		return _materialAttachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	const MaterialCompilationAttachment *_materialAttachment = nullptr;
};

class MaterialCompilationRenderPassHandle : public QueuePassHandle {
public:
	virtual ~MaterialCompilationRenderPassHandle();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

	Rc<gl::MaterialSet> _outputData;
	MaterialCompilationAttachmentHandle *_materialAttachment;
};

MaterialCompiler::~MaterialCompiler() { }

bool MaterialCompiler::init() {
	Queue::Builder builder("Material");

	auto attachment = Rc<MaterialCompilationAttachment>::create("MaterialAttachment");
	auto pass = Rc<MaterialCompilationRenderPass>::create("MaterialRenderPass");

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

bool MaterialCompiler::inProgress(const gl::MaterialAttachment *a) const {
	auto it = _inProgress.find(a);
	if (it != _inProgress.end()) {
		return true;
	}
	return false;
}

void MaterialCompiler::setInProgress(const gl::MaterialAttachment *a) {
	_inProgress.emplace(a);
}

void MaterialCompiler::dropInProgress(const gl::MaterialAttachment *a) {
	_inProgress.erase(a);
}

bool MaterialCompiler::hasRequest(const gl::MaterialAttachment *a) const {
	auto it = _requests.find(a);
	if (it != _requests.end()) {
		return true;
	}
	return false;
}

void MaterialCompiler::appendRequest(const gl::MaterialAttachment *a, Rc<gl::MaterialInputData> &&req,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) {
	auto it = _requests.find(a);
	if (it == _requests.end()) {
		it = _requests.emplace(a, MaterialRequest()).first;
	}

	for (auto &rem : req->materialsToRemove) {
		auto m = it->second.materials.find(rem);
		if (m != it->second.materials.end()) {
			it->second.materials.erase(m);
		}

		auto d = it->second.dynamic.find(rem);
		if (d != it->second.dynamic.end()) {
			it->second.dynamic.erase(d);
		}

		it->second.remove.emplace(rem);
	}

	for (auto &rem : req->dynamicMaterialsToUpdate) {
		it->second.dynamic.emplace(rem);
	}

	for (auto &m : req->materialsToAddOrUpdate) {
		auto materialId = m->getId();
		auto iit = it->second.materials.find(materialId);
		if (iit == it->second.materials.end()) {
			it->second.materials.emplace(materialId, move(m));
		} else {
			iit->second = move(m);
		}
		auto v = it->second.remove.find(materialId);
		if (v != it->second.remove.end()) {
			it->second.remove.erase(v);
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

void MaterialCompiler::clearRequests() {
	_requests.clear();
}

auto MaterialCompiler::makeRequest(Rc<gl::MaterialInputData> &&input,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) -> Rc<FrameRequest> {
	auto req = Rc<FrameRequest>::create(this);
	req->addInput(_attachment, move(input));
	req->addSignalDependencies(move(deps));
	return req;
}

void MaterialCompiler::runMaterialCompilationFrame(gl::Loop &loop, Rc<gl::MaterialInputData> &&req,
		Vector<Rc<renderqueue::DependencyEvent>> &&deps) {
	auto targetAttachment = req->attachment;

	auto h = loop.makeFrame(makeRequest(move(req), move(deps)), 0);
	h->setCompleteCallback([this, targetAttachment] (FrameHandle &handle) {
		auto reqIt = _requests.find(targetAttachment);
		if (reqIt != _requests.end()) {
			if (handle.getLoop()->isRunning()) {
				auto deps = move(reqIt->second.deps);
				Rc<gl::MaterialInputData> req = Rc<gl::MaterialInputData>::alloc();
				req->attachment = targetAttachment;
				req->materialsToAddOrUpdate.reserve(reqIt->second.materials.size());
				for (auto &m : reqIt->second.materials) {
					req->materialsToAddOrUpdate.emplace_back(m.second);
				}
				req->materialsToRemove.reserve(reqIt->second.remove.size());
				for (auto &m : reqIt->second.remove) {
					req->materialsToRemove.emplace_back(m);
				}
				req->dynamicMaterialsToUpdate.reserve(reqIt->second.dynamic.size());
				for (auto &m : reqIt->second.dynamic) {
					req->dynamicMaterialsToUpdate.emplace_back(m);
				}
				_requests.erase(reqIt);

				runMaterialCompilationFrame(*handle.getLoop(), move(req), move(deps));
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

MaterialCompilationAttachment::~MaterialCompilationAttachment() { }

auto MaterialCompilationAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MaterialCompilationAttachmentHandle>::create(this, handle);
}

MaterialCompilationAttachmentHandle::~MaterialCompilationAttachmentHandle() { }

bool MaterialCompilationAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	return true;
}

void MaterialCompilationAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::MaterialInputData>();
	if (!d || q.isFinalized()) {
		cb(false);
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) {
		handle.performOnGlThread([this, d = move(d), cb = move(cb)] (FrameHandle &handle) {
			_inputData = d;
			_originalSet = _inputData->attachment->getMaterials();
			cb(true);
		}, this, true, "MaterialCompilationAttachmentHandle::submitInput");
	});
}

MaterialCompilationRenderPass::~MaterialCompilationRenderPass() { }

bool MaterialCompilationRenderPass::init(StringView name) {
	if (QueuePass::init(name, gl::RenderPassType::Generic, renderqueue::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

auto MaterialCompilationRenderPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialCompilationRenderPassHandle>::create(*this, handle);
}

void MaterialCompilationRenderPass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<MaterialCompilationAttachment *>(it->getAttachment())) {
			_materialAttachment = a;
		}
	}
}

MaterialCompilationRenderPassHandle::~MaterialCompilationRenderPassHandle() { }

bool MaterialCompilationRenderPassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(((MaterialCompilationRenderPass *)_renderPass.get())->getMaterialAttachment())) {
		_materialAttachment = (MaterialCompilationAttachmentHandle *)a->handle.get();
	}

	auto &originalData = _materialAttachment->getOriginalSet();
	auto &inputData = _materialAttachment->getInputData();
	_outputData = inputData->attachment->cloneSet(originalData);

	return QueuePassHandle::prepare(frame, move(cb));
}

void MaterialCompilationRenderPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);
}

Vector<VkCommandBuffer> MaterialCompilationRenderPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);
	auto layout = _device->getTextureSetLayout();

	auto &inputData = _materialAttachment->getInputData();
	auto buffers = updateMaterials(handle, _outputData, inputData->materialsToAddOrUpdate,
			inputData->dynamicMaterialsToUpdate, inputData->materialsToRemove);
	if (!buffers.targetBuffer) {
		return Vector<VkCommandBuffer>();
	}

	QueueOperations ops = QueueOperations::None;
	for (auto &it : inputData->attachment->getRenderPasses()) {
		ops |= ((QueuePass *)it->renderPass.get())->getQueueOps();
	}

	auto q = _device->getQueueFamily(ops);
	if (!q) {
		return Vector<VkCommandBuffer>();
	}

	// transition images and build buffer
	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	VkBufferCopy indexesCopy;
	indexesCopy.srcOffset = 0;
	indexesCopy.dstOffset = 0;
	indexesCopy.size = buffers.stagingBuffer->getSize();

	table->vkCmdCopyBuffer(buf, buffers.stagingBuffer->getBuffer(), buffers.targetBuffer->getBuffer(), 1, &indexesCopy);

	VkPipelineStageFlags targetStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if (_pool->getClass() == QueueOperations::Transfer) {
		targetStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	if (q->index == _pool->getFamilyIdx()) {
		VkBufferMemoryBarrier bufferBarrier({
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			buffers.targetBuffer->getBuffer(), 0, VK_WHOLE_SIZE
		});

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
				targetStages, 0,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
	} else {
		VkBufferMemoryBarrier bufferBarrier({
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			_pool->getFamilyIdx(), q->index,
			buffers.targetBuffer->getBuffer(), 0, VK_WHOLE_SIZE
		});

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
				targetStages, 0,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);

		buffers.targetBuffer->setPendingBarrier(bufferBarrier);
	}

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		auto tmpBuffer = new Rc<Buffer>(move(buffers.targetBuffer));
		auto tmpOrder = new std::unordered_map<gl::MaterialId, uint32_t>(move(buffers.ordering));
		handle.performOnGlThread([data = _outputData, tmpBuffer, tmpOrder] (FrameHandle &) {
			data->setBuffer(move(*tmpBuffer), move(*tmpOrder));
			delete tmpBuffer;
			delete tmpOrder;
		}, nullptr, true, "MaterialCompilationRenderPassHandle::doPrepareCommands");
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

void MaterialCompilationRenderPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	if (success) {
		_materialAttachment->getInputData()->attachment->setMaterials(_outputData);
	}

	QueuePassHandle::doComplete(queue, move(func), success);
}

}
