/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkRenderQueueAttachment.h"
#include "XLGlFrame.h"
#include "XLGlRenderQueue.h"
#include "XLVkPipeline.h"
#include "XLVkRenderPassImpl.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

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
				fail();
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
				fail();
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
						fail();
						return false;
					} else {
						pipeline->pipeline = ret.get();
						if (_pipelinesInQueue.fetch_sub(1) == 1) {
							complete();
						}
					}
					return true;
				}, this);
			}
		}
	}

	if (tasksCount == 0) {
		complete();
	}
}

void RenderQueueAttachmentHandle::fail() {

}

void RenderQueueAttachmentHandle::complete() {

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

RenderQueueRenderPassHandle::~RenderQueueRenderPassHandle() { }

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
			if (!_resource->prepareCommands(_pool->getFamilyIdx(), buf)) {
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

void RenderQueueRenderPassHandle::submit(gl::FrameHandle &frame, Function<void(const Rc<gl::RenderPass> &)> &&func) {
	if (_buffers.empty()) {
		func(_renderPass);
	} else {
		RenderPassHandle::submit(frame, move(func));
	}
}

void RenderQueueRenderPassHandle::addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) {
	RenderPassHandle::addRequiredAttachment(a, h);
	if (a == ((RenderQueueRenderPass *)_renderPass.get())->getAttachment()) {
		_attachment = (RenderQueueAttachmentHandle *)h.get();
	}
}

}
