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

#include "XLGlFrame.h"
#include "XLGlRenderQueue.h"
#include "XLVkPipeline.h"
#include "XLVkRenderPassImpl.h"
#include "XLGlRenderQueue.h"
#include "XLVkRenderQueueCompiler.h"

namespace stappler::xenolith::vk {

class RenderQueueAttachment : public gl::GenericAttachment {
public:
	virtual ~RenderQueueAttachment();

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &) override;
};

class RenderQueueAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~RenderQueueAttachmentHandle();

	virtual bool setup(gl::FrameHandle &handle) override;
	virtual bool submitInput(gl::FrameHandle &, Rc<gl::AttachmentInputData> &&) override;

	const Rc<gl::RenderQueue> &getRenderQueue() const { return _input->queue; }
	const Rc<TransferResource> &getTransferResource() const { return _resource; }

protected:
	void runShaders(gl::FrameHandle &frame);
	void runPipelines(gl::FrameHandle &frame);

	Device *_device = nullptr;
	std::atomic<size_t> _programsInQueue = 0;
	std::atomic<size_t> _pipelinesInQueue = 0;
	Rc<TransferResource> _resource;
	Rc<RenderQueueInput> _input;
};

class RenderQueueRenderPass : public RenderPass {
public:
	virtual ~RenderQueueRenderPass();

	virtual bool init(StringView);

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &) override;

	const RenderQueueAttachment *getAttachment() const {
		return _attachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	RenderQueueAttachment *_attachment = nullptr;
};

class RenderQueueRenderPassHandle : public RenderPassHandle {
public:
	virtual ~RenderQueueRenderPassHandle();

	virtual bool prepare(gl::FrameHandle &) override;
	virtual void submit(gl::FrameHandle &, Function<void(const Rc<gl::RenderPassHandle> &)> &&) override;

	virtual void finalize(gl::FrameHandle &, bool successful) override;

protected:
	virtual void addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) override;

	virtual bool prepareMaterials(gl::FrameHandle &frame, VkCommandBuffer buf,
			const Rc<gl::MaterialAttachment> &attachment, Vector<VkBufferMemoryBarrier> &outputBufferBarriers);

	Rc<TransferResource> _resource;
	Rc<gl::RenderQueue> _queue;
	RenderQueueAttachmentHandle *_attachment;
};

RenderQueueCompiler::~RenderQueueCompiler() { }

bool RenderQueueCompiler::init(Device &dev) {
	gl::RenderQueue::Builder builder("RenderQueue", gl::RenderQueue::RenderOnDemand);

	auto attachment = Rc<RenderQueueAttachment>::create("RenderQueueAttachment");
	auto pass = Rc<RenderQueueRenderPass>::create("RenderQueueRenderPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment);
	builder.addPassOutput(pass, 0, attachment);
	builder.addInput(attachment);
	builder.addOutput(attachment);

	if (gl::RenderQueue::init(move(builder))) {
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

void RenderQueueCompiler::submitInput(gl::FrameHandle &frame, Rc<RenderQueueInput> &&input) {
	frame.submitInput(_attachment, move(input), true);
}

RenderQueueAttachment::~RenderQueueAttachment() { }

Rc<gl::AttachmentHandle> RenderQueueAttachment::makeFrameHandle(const gl::FrameHandle &handle) {
	return Rc<RenderQueueAttachmentHandle>::create(this, handle);
}

RenderQueueAttachmentHandle::~RenderQueueAttachmentHandle() { }

bool RenderQueueAttachmentHandle::setup(gl::FrameHandle &handle) {
	_device = (Device *)handle.getDevice();
	return true;
}

bool RenderQueueAttachmentHandle::submitInput(gl::FrameHandle &frame, Rc<gl::AttachmentInputData> &&data) {
	_input = (RenderQueueInput *)data.get();

	if (_input->queue->getInternalResource()) {
		frame.performInQueue([this] (gl::FrameHandle &frame) -> bool {
			runShaders(frame);
			_resource = Rc<TransferResource>::create(_device->getAllocator(), _input->queue->getInternalResource());
			if (_resource->initialize()) {
				return true;
			}
			return false;
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (success) {
				frame.setInputSubmitted(this);
			} else {
				frame.invalidate();
			}
		});
	} else {
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.setInputSubmitted(this);
			runShaders(frame);
		}, this);
	}
	return true;
}

void RenderQueueAttachmentHandle::runShaders(gl::FrameHandle &frame) {
	size_t tasksCount = 0;
	Vector<gl::ProgramData *> programs;

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
		frame.performRequiredTask([this, req = it] (gl::FrameHandle &frame) {
			auto ret = Rc<Shader>::create(*_device, *req);
			if (!ret) {
				log::vtext("Gl-Device", "Fail to compile shader program ", req->key);
			} else {
				req->program = _device->addProgram(ret);
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
		}, this);
	}

	_input->queue->prepare(*_device);

	for (auto &it : _input->queue->getPasses()) {
		frame.performRequiredTask([this, req = it] (gl::FrameHandle &frame) -> bool {
			auto ret = Rc<RenderPassImpl>::create(*_device, *req);
			if (!ret) {
				log::vtext("Gl-Device", "Fail to compile render pass ", req->key);
			} else {
				req->impl = ret.get();
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
			return true;
		}, this);
	}

	if (tasksCount == 0) {
		runPipelines(frame);
	}
}

void RenderQueueAttachmentHandle::runPipelines(gl::FrameHandle &frame) {
	size_t tasksCount = _pipelinesInQueue.load();
	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			_pipelinesInQueue += sit.pipelines.size();
			tasksCount += sit.pipelines.size();
		}
	}

	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			for (auto &it : sit.pipelines) {
				frame.performRequiredTask([this, pass = &sit, pipeline = it] (gl::FrameHandle &frame) -> bool {
					auto ret = Rc<Pipeline>::create(*_device, *pipeline, *pass, *_input->queue);
					if (!ret) {
						log::vtext("Gl-Device", "Fail to compile pipeline ", pipeline->key);
						return false;
					} else {
						pipeline->pipeline = ret.get();
					}
					return true;
				}, this);
			}
		}
	}
}

RenderQueueRenderPass::~RenderQueueRenderPass() { }

bool RenderQueueRenderPass::init(StringView name) {
	if (RenderPass::init(name, gl::RenderPassType::Transfer, gl::RenderOrderingHighest, 1)) {
		_queueOps = QueueOperations::Transfer;
		return true;
	}
	return false;
}

Rc<gl::RenderPassHandle> RenderQueueRenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<RenderQueueRenderPassHandle>::create(*this, data, handle);
}

void RenderQueueRenderPass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<RenderQueueAttachment *>(it->getAttachment())) {
			_attachment = a;
		}
	}
}

RenderQueueRenderPassHandle::~RenderQueueRenderPassHandle() {
	if (_resource) {
		_resource->invalidate(*_device);
	}
}

bool RenderQueueRenderPassHandle::prepare(gl::FrameHandle &frame) {
	_device = (Device *)frame.getDevice();
	_queue = _attachment->getRenderQueue();

	if (auto &res = _attachment->getTransferResource()) {
		_resource = res;
		_pool = _device->acquireCommandPool(QueueOperations::Transfer);
		if (!_pool) {
			invalidate();
			return false;
		}

		frame.performInQueue([this] (gl::FrameHandle &frame) {
			auto buf = _pool->allocBuffer(*_device);
			auto table = _device->getTable();

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			table->vkBeginCommandBuffer(buf, &beginInfo);

			Vector<VkImageMemoryBarrier> outputImageBarriers;
			Vector<VkBufferMemoryBarrier> outputBufferBarriers;

			if (!_resource->prepareCommands(_pool->getFamilyIdx(), buf, outputImageBarriers, outputBufferBarriers)) {
				return false;
			}

			_resource->compile();

			for (auto &it : _queue->getAttachments()) {
				if (auto v = it.cast<gl::MaterialAttachment>()) {
					if (!prepareMaterials(frame, buf, v, outputBufferBarriers)) {
						return true;
					}
				}
			}

			table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				outputBufferBarriers.size(), outputBufferBarriers.data(),
				outputImageBarriers.size(), outputImageBarriers.data());

			if (table->vkEndCommandBuffer(buf) != VK_SUCCESS) {
				return false;
			}

			_buffers.emplace_back(buf);
			return true;
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (success) {
				_commandsReady = true;
				_descriptorsReady = true;
				frame.setRenderPassPrepared(this);
			} else {
				log::vtext("VK-Error", "Fail to doPrepareCommands");
				frame.invalidate();
			}
		}, this, "RenderPass::doPrepareCommands");
	} else {
		frame.performOnGlThread([&] (gl::FrameHandle &frame) {
			frame.setRenderPassPrepared(this);
		}, this, false);
	}

	return true;
}

void RenderQueueRenderPassHandle::submit(gl::FrameHandle &frame, Function<void(const Rc<gl::RenderPassHandle> &)> &&func) {
	if (_buffers.empty()) {
		frame.setRenderPassSubmitted(this);
		func(this);
	} else {
		RenderPassHandle::submit(frame, move(func));
	}
}

void RenderQueueRenderPassHandle::finalize(gl::FrameHandle &frame, bool successful) {
	RenderPassHandle::finalize(frame, successful);
	_attachment->getRenderQueue()->setCompiled(true);
}

void RenderQueueRenderPassHandle::addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) {
	RenderPassHandle::addRequiredAttachment(a, h);
	if (a == ((RenderQueueRenderPass *)_renderPass.get())->getAttachment()) {
		_attachment = (RenderQueueAttachmentHandle *)h.get();
	}
}

bool RenderQueueRenderPassHandle::prepareMaterials(gl::FrameHandle &iframe, VkCommandBuffer buf,
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
		ops |= ((RenderPass *)it->renderPass.get())->getQueueOps();
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
		iframe.performOnGlThread([attachment, data, tmpBuffer, tmpOrder] (gl::FrameHandle &) {
			data->setBuffer(move(*tmpBuffer), move(*tmpOrder));
			attachment->setMaterials(data);
			delete tmpBuffer;
			delete tmpOrder;
		});

		return true;
	}
	return false;
}

}
