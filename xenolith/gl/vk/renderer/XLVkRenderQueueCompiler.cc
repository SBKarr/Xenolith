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

#include "XLRenderQueueAttachment.h"
#include "XLVkPipeline.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkRenderQueueCompiler.h"

namespace stappler::xenolith::vk {

class RenderQueueAttachment : public renderqueue::GenericAttachment {
public:
	virtual ~RenderQueueAttachment();

	virtual Rc<renderqueue::AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class RenderQueueAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~RenderQueueAttachmentHandle();

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	const Rc<renderqueue::Queue> &getRenderQueue() const { return _input->queue; }
	const Rc<TransferResource> &getTransferResource() const { return _resource; }

protected:
	void runShaders(FrameHandle &frame);
	void runPipelines(FrameHandle &frame);

	Device *_device = nullptr;
	std::atomic<size_t> _programsInQueue = 0;
	std::atomic<size_t> _pipelinesInQueue = 0;
	Rc<TransferResource> _resource;
	Rc<RenderQueueInput> _input;
};

class RenderQueuePass : public QueuePass {
public:
	virtual ~RenderQueuePass();

	virtual bool init(StringView);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const RenderQueueAttachment *getAttachment() const {
		return _attachment;
	}

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	RenderQueueAttachment *_attachment = nullptr;
};

class RenderQueuePassHandle : public QueuePassHandle {
public:
	virtual ~RenderQueuePassHandle();

	virtual bool init(Pass &, const FrameQueue &) override;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

	virtual void finalize(FrameQueue &, bool successful) override;

protected:
	virtual bool prepareMaterials(FrameHandle &frame, VkCommandBuffer buf,
			const Rc<gl::MaterialAttachment> &attachment, Vector<VkBufferMemoryBarrier> &outputBufferBarriers);

	Rc<TransferResource> _resource;
	Rc<renderqueue::Queue> _queue;
	RenderQueueAttachmentHandle *_attachment;
};

RenderQueueCompiler::~RenderQueueCompiler() { }

bool RenderQueueCompiler::init(Device &dev) {
	renderqueue::Queue::Builder builder("RenderQueue");

	auto attachment = Rc<RenderQueueAttachment>::create("RenderQueueAttachment");
	auto pass = Rc<RenderQueuePass>::create("RenderQueueRenderPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addInput(attachment);
	builder.addOutput(attachment);

	if (Queue::init(move(builder))) {
		_attachment = attachment.get();

		prepare(dev);

		for (auto &it : getPasses()) {
			auto pass = Rc<RenderPassImpl>::create(dev, *it);
			it->impl = pass.get();
		}

		return true;
	}
	return false;
}

auto RenderQueueCompiler::makeRequest(Rc<RenderQueueInput> &&input) -> Rc<FrameRequest> {
	auto ret = Rc<FrameRequest>::create(this);
	ret->addInput(_attachment, move(input));
	return ret;
}

RenderQueueAttachment::~RenderQueueAttachment() { }

auto RenderQueueAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<RenderQueueAttachmentHandle>::create(this, handle);
}

RenderQueueAttachmentHandle::~RenderQueueAttachmentHandle() { }

bool RenderQueueAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&) {
	_device = (Device *)handle.getFrame()->getDevice();
	return true;
}

void RenderQueueAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	_input = (RenderQueueInput *)data.get();
	if (!_input || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, cb = move(cb)] (FrameHandle &handle, bool success) {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		if (_input->queue->getInternalResource()) {
			handle.performInQueue([this] (FrameHandle &frame) -> bool {
				runShaders(frame);
				_resource = Rc<TransferResource>::create(_device->getAllocator(), _input->queue->getInternalResource());
				if (_resource->initialize()) {
					return true;
				}
				return false;
			}, [cb = move(cb)] (FrameHandle &frame, bool success) {
				cb(success);
			}, nullptr, "RenderQueueAttachmentHandle::submitInput _input->queue->getInternalResource");
		} else {
			handle.performOnGlThread([this, cb = move(cb)] (FrameHandle &frame) {
				cb(true);
				runShaders(frame);
			}, this, true, "RenderQueueAttachmentHandle::submitInput");
		}
	});
}

void RenderQueueAttachmentHandle::runShaders(FrameHandle &frame) {
	size_t tasksCount = 0;
	Vector<renderqueue::ProgramData *> programs;

	// count phase-1 tasks
	_programsInQueue += _input->queue->getPasses().size();
	tasksCount += _input->queue->getPasses().size();

	for (auto &it : _input->queue->getPrograms()) {
		if (auto p = _device->getProgram(it->key)) {
			it->program = p;
		} else {
			++ tasksCount;
			++ _programsInQueue;
			programs.emplace_back(it);
		}
	}

	for (auto &it : programs) {
		frame.performRequiredTask([this, req = it] (FrameHandle &frame) {
			auto ret = Rc<Shader>::create(*_device, *req);
			if (!ret) {
				log::vtext("Gl-Device", "Fail to compile shader program ", req->key);
				return false;
			} else {
				req->program = _device->addProgram(ret);
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
			return true;
		}, this, "RenderQueueAttachmentHandle::runShaders - programs");
	}

	_input->queue->prepare(*_device);

	for (auto &it : _input->queue->getPasses()) {
		frame.performRequiredTask([this, req = it] (FrameHandle &frame) -> bool {
			auto ret = Rc<RenderPassImpl>::create(*_device, *req);
			if (!ret) {
				log::vtext("Gl-Device", "Fail to compile render pass ", req->key);
				return false;
			} else {
				req->impl = ret.get();
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
			return true;
		}, this, "RenderQueueAttachmentHandle::runShaders - passes");
	}

	if (tasksCount == 0) {
		runPipelines(frame);
	}
}

void RenderQueueAttachmentHandle::runPipelines(FrameHandle &frame) {
	[[maybe_unused]] size_t tasksCount = _pipelinesInQueue.load();
	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			_pipelinesInQueue += sit.graphicPipelines.size() + sit.computePipelines.size();
			tasksCount += sit.graphicPipelines.size() + sit.computePipelines.size();
		}
	}

	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			for (auto &it : sit.graphicPipelines) {
				frame.performRequiredTask([this, pass = &sit, pipeline = it] (FrameHandle &frame) -> bool {
					auto ret = Rc<GraphicPipeline>::create(*_device, *pipeline, *pass, *_input->queue);
					if (!ret) {
						log::vtext("Gl-Device", "Fail to compile pipeline ", pipeline->key);
						return false;
					} else {
						pipeline->pipeline = ret.get();
					}
					return true;
				}, this, "RenderQueueAttachmentHandle::runPipelines");
			}
			for (auto &it : sit.computePipelines) {
				frame.performRequiredTask([this, pass = &sit, pipeline = it] (FrameHandle &frame) -> bool {
					auto ret = Rc<ComputePipeline>::create(*_device, *pipeline, *pass, *_input->queue);
					if (!ret) {
						log::vtext("Gl-Device", "Fail to compile pipeline ", pipeline->key);
						return false;
					} else {
						pipeline->pipeline = ret.get();
					}
					return true;
				}, this, "RenderQueueAttachmentHandle::runPipelines");
			}
		}
	}
}

RenderQueuePass::~RenderQueuePass() { }

bool RenderQueuePass::init(StringView name) {
	if (QueuePass::init(name, PassType::Transfer, renderqueue::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

auto RenderQueuePass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<RenderQueuePassHandle>::create(*this, handle);
}

void RenderQueuePass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<RenderQueueAttachment *>(it->getAttachment())) {
			_attachment = a;
		}
	}
}

RenderQueuePassHandle::~RenderQueuePassHandle() {
	if (_resource) {
		_resource->invalidate(*_device);
	}
}

bool RenderQueuePassHandle::init(Pass &pass, const FrameQueue &queue) {
	if (!QueuePassHandle::init(pass, queue)) {
		return false;
	}

	_isAsync = true;
	return true;
}

bool RenderQueuePassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(((RenderQueuePass *)_renderPass.get())->getAttachment())) {
		_attachment = (RenderQueueAttachmentHandle *)a->handle.get();
	}

	_loop = (Loop *)frame.getLoop();
	_device = (Device *)frame.getFrame()->getDevice();
	_queue = _attachment->getRenderQueue();

	auto hasMaterials = false;
	auto &res = _attachment->getTransferResource();
	for (auto &it : _queue->getAttachments()) {
		if (auto v = it.cast<gl::MaterialAttachment>()) {
			if (!v->getInitialMaterials().empty()) {
				hasMaterials = true;
				break;
			}
		}
	}

	if (hasMaterials || res) {
		_resource = res;
		_pool = _device->acquireCommandPool(QueueOperations::Transfer);
		if (!_pool) {
			invalidate();
			return false;
		}

		frame.getFrame()->performInQueue([this, hasMaterials] (FrameHandle &frame) {
			auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
				Vector<VkImageMemoryBarrier> outputImageBarriers;
				Vector<VkBufferMemoryBarrier> outputBufferBarriers;

				if (_resource) {
					if (!_resource->prepareCommands(_pool->getFamilyIdx(), buf.getBuffer(), outputImageBarriers, outputBufferBarriers)) {
						log::vtext("vk::RenderQueueCompiler", "Fail to compile resource for ", _queue->getName());
						return false;
					}
					_resource->compile();
				}

				if (hasMaterials) {
					for (auto &it : _queue->getAttachments()) {
						if (auto v = it.cast<gl::MaterialAttachment>()) {
							if (!prepareMaterials(frame, buf.getBuffer(), v, outputBufferBarriers)) {
								log::vtext("vk::RenderQueueCompiler", "Fail to compile predefined materials for ", _queue->getName());
								return false;
							}
						}
					}
				}

				_device->getTable()->vkCmdPipelineBarrier(buf.getBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
					0, nullptr,
					outputBufferBarriers.size(), outputBufferBarriers.data(),
					outputImageBarriers.size(), outputImageBarriers.data());
				return true;
			});

			if (buf) {
				_buffers.emplace_back(buf->getBuffer());
			}
			return true;
		}, [this, cb = move(cb)] (FrameHandle &frame, bool success) {
			if (success) {
				_commandsReady = true;
				_descriptorsReady = true;
			} else {
				log::vtext("VK-Error", "Fail to doPrepareCommands");
			}
			cb(success);
		}, this, "RenderPass::doPrepareCommands _attachment->getTransferResource");
	} else {
		frame.getFrame()->performOnGlThread([cb = move(cb)] (FrameHandle &frame) {
			cb(true);
		}, this, false, "RenderPass::doPrepareCommands");
	}

	return false;
}

void RenderQueuePassHandle::submit(FrameQueue &queue, Rc<FrameSync> &&sync, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	if (_buffers.empty()) {
		onSubmited(true);
		onComplete(true);
	} else {
		QueuePassHandle::submit(queue, move(sync), move(onSubmited), move(onComplete));
	}
}

void RenderQueuePassHandle::finalize(FrameQueue &frame, bool successful) {
	QueuePassHandle::finalize(frame, successful);
	Vector<uint64_t> ids;
	auto &cache = frame.getLoop()->getFrameCache();
	for (auto &it : _attachment->getRenderQueue()->getPasses()) {
		if (it->impl && it->renderPass->getType() != gl::RenderPassType::Generic) {
			ids.emplace_back(it->impl->getIndex());
			cache->addRenderPass(it->impl->getIndex());
		}
	}
	_attachment->getRenderQueue()->setCompiled(true, [loop = Rc<gl::Loop>(frame.getLoop()), ids = move(ids)] {
		loop->performOnGlThread([loop, ids] {
			auto &cache = loop->getFrameCache();
			for (auto &id : ids) {
				cache->removeRenderPass(id);
			}
		});
	});
}

bool RenderQueuePassHandle::prepareMaterials(FrameHandle &iframe, VkCommandBuffer buf,
		const Rc<gl::MaterialAttachment> &attachment, Vector<VkBufferMemoryBarrier> &outputBufferBarriers) {
	auto table = _device->getTable();
	auto &initial = attachment->getInitialMaterials();
	if (initial.empty()) {
		return true;
	}

	auto data = attachment->allocateSet(*_device);

	auto buffers = updateMaterials(iframe, data, initial, SpanView<gl::MaterialId>(), SpanView<gl::MaterialId>());

	VkBufferCopy indexesCopy;
	indexesCopy.srcOffset = 0;
	indexesCopy.dstOffset = 0;
	indexesCopy.size = buffers.stagingBuffer->getSize();

	auto stagingBuf = buffers.stagingBuffer->getBuffer();
	auto targetBuf = buffers.targetBuffer->getBuffer();

	Vector<VkImageMemoryBarrier> outputImageBarriers;
	table->vkCmdCopyBuffer(buf, stagingBuf, targetBuf, 1, &indexesCopy);

	QueueOperations ops = QueueOperations::None;
	for (auto &it : attachment->getRenderPasses()) {
		ops |= ((QueuePass *)it->renderPass.get())->getQueueOps();
	}

	if (auto q = _device->getQueueFamily(ops)) {
		if (q->index == _pool->getFamilyIdx()) {
			outputBufferBarriers.emplace_back(VkBufferMemoryBarrier({
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				buffers.targetBuffer->getBuffer(), 0, VK_WHOLE_SIZE
			}));
		} else {
			auto &b = outputBufferBarriers.emplace_back(VkBufferMemoryBarrier({
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				_pool->getFamilyIdx(), q->index,
				buffers.targetBuffer->getBuffer(), 0, VK_WHOLE_SIZE
			}));
			buffers.targetBuffer->setPendingBarrier(b);
		}

		auto tmpBuffer = new Rc<Buffer>(move(buffers.targetBuffer));
		auto tmpOrder = new std::unordered_map<gl::MaterialId, uint32_t>(move(buffers.ordering));
		iframe.performOnGlThread([attachment, data, tmpBuffer, tmpOrder] (FrameHandle &) {
			data->setBuffer(move(*tmpBuffer), move(*tmpOrder));
			attachment->setMaterials(data);
			delete tmpBuffer;
			delete tmpOrder;
		}, nullptr, false, "RenderQueueRenderPassHandle::prepareMaterials");

		return true;
	}
	return false;
}

}
