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

#include "XLVkRenderPass.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"
#include "XLGlFrame.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::vk {

RenderPass::~RenderPass() { }

void RenderPass::invalidate() { }

Rc<gl::RenderPassHandle> RenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<RenderPassHandle>::create(*this, data, handle);
}

RenderPassHandle::~RenderPassHandle() {
	invalidate();
}

void RenderPassHandle::invalidate() {
	if (_pool) {
		_device->releaseCommandPool(move(_pool));
		_pool = nullptr;
	}

	if (_fence) {
		_device->releaseFence(move(_fence));
		_fence = nullptr;
	}

	if (_queue) {
		_device->releaseQueue(move(_queue));
		_queue = nullptr;
	}

	_sync.waitAttachment.clear();
	_sync.waitSem.clear();
	_sync.waitStages.clear();
	_sync.signalSem.clear();
	_sync.signalAttachment.clear();
}

bool RenderPassHandle::prepare(gl::FrameHandle &frame) {
	_device = (Device *)frame.getDevice();
	_pool = _device->acquireCommandPool(QueueOperations::Graphics);
	if (!_pool) {
		invalidate();
		return false;
	}

	uint32_t index = 0;
	for (auto &it : _attachments) {
		if (it->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
			auto img = it.cast<SwapchainAttachmentHandle>();
			index = img->getIndex();
		}
	}

	if (index == maxOf<uint32_t>()) {
		invalidate();
		return false;
	}

	frame.performInQueue([this, index] (gl::FrameHandle &frame) {
		auto table = _device->getTable();
		auto buf = _pool->allocBuffer(*_device);


		auto targetFb = _data->framebuffers[index].cast<Framebuffer>();
		auto currentExtent = targetFb->getExtent();

		VkCommandBufferBeginInfo beginInfo { };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;

		if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
			return false;
		}

		VkRenderPassBeginInfo renderPassInfo { };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _data->impl.cast<RenderPassImpl>()->getRenderPass();
		renderPassInfo.framebuffer = targetFb->getFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		table->vkCmdBeginRenderPass(buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		table->vkCmdSetViewport(buf, 0, 1, &viewport);

		VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

		auto pipeline = _data->pipelines.get(StringView("Default"));

		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());
		table->vkCmdDraw(buf, 3, 1, 0, 0);
		table->vkCmdEndRenderPass(buf);
		if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
			_buffers.emplace_back(buf);
			return true;
		}
		return false;
	}, [this] (gl::FrameHandle &frame, bool success) {
		if (success) {
			frame.setRenderPassPrepared(this);
		} else {
			log::vtext("VK-Error", "Fail to vkEndCommandBuffer");
			frame.invalidate();
		}
	}, this);
	return false;
}

void RenderPassHandle::submit(gl::FrameHandle &frame, Function<void(const Rc<gl::RenderPass> &)> &&func) {
	Rc<gl::FrameHandle> f = &frame; // capture frame ref

	_fence = _device->acquireFence(frame.getOrder());
	_fence->addRelease([dev = _device, pool = move(_pool)] {
		dev->releaseCommandPool(Rc<CommandPool>(pool));
	});
	_fence->addRelease([func = move(func), pass = _renderPass] {
		func(pass);
	});

	_pool = nullptr;
	_sync = makeSyncInfo();
	for (auto &it : _sync.swapchainSync) {
		_fence->addRelease([dev = _device, sync = it] {
			dev->releaseSwapchainSync(Rc<SwapchainSync>(sync));
		});
	}

	_device->acquireQueue(QueueOperations::Graphics, frame, [this] (gl::FrameHandle &frame, const Rc<DeviceQueue> &queue) {
		_queue = queue;
		if (_data->isPresentable) {
			for (auto &it : _attachments) {
				if (it->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
					if (auto d = it.cast<SwapchainAttachmentHandle>()) {
						_presentAttachment = d;
					}
				}
			}
		}

		frame.performInQueue([this] (gl::FrameHandle &frame) {
			auto table = _device->getTable();
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			submitInfo.waitSemaphoreCount = _sync.waitSem.size();
			submitInfo.pWaitSemaphores = _sync.waitSem.data();
			submitInfo.pWaitDstStageMask = _sync.waitStages.data();
			submitInfo.commandBufferCount = _buffers.size();
			submitInfo.pCommandBuffers = _buffers.data();
			submitInfo.signalSemaphoreCount = _sync.signalSem.size();
			submitInfo.pSignalSemaphores = _sync.signalSem.data();

			if (table->vkQueueSubmit(_queue->getQueue(), 1, &submitInfo, _fence->getFence()) != VK_SUCCESS) {
				return false;
			}

			if (_presentAttachment) {
				if ((_queue->getOps() & QueueOperations::Present) != QueueOperations::None) {
					present(frame);
				} else {
					// TODO - present on another queue
				}
			}

			return true;
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (success) {
				if (_queue) {
					_device->releaseQueue(move(_queue));
					_queue = nullptr;
				}
				_device->scheduleFence(*frame.getLoop(), move(_fence));
				_fence = nullptr;
				frame.setRenderPassSubmitted(this);
				invalidate();
			} else {
				if (_queue) {
					_device->releaseQueue(move(_queue));
					_queue = nullptr;
				}
				log::vtext("VK-Error", "Fail to vkQueueSubmit");
				_device->releaseFence(move(_fence));
				_fence = nullptr;
				invalidate();
				frame.invalidate();
			}
		}, this);
	}, [this] (gl::FrameHandle &frame) {
		invalidate();
	}, this);
}

bool RenderPassHandle::present(gl::FrameHandle &frame) {
	auto table = _device->getTable();

	Vector<VkSemaphore> presentSem;

	auto imageIndex = _presentAttachment->getIndex();
	for (auto &it : _sync.swapchainSync) {
		presentSem.emplace_back(it->getRenderFinished()->getSemaphore());
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = presentSem.size();
	presentInfo.pWaitSemaphores = presentSem.data();

	VkSwapchainKHR swapChains[] = {_presentAttachment->getSwapchain()->getSwapchain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	// auto t = platform::device::_clock();
	auto result = table->vkQueuePresentKHR(_queue->getQueue(), &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.getLoop()->pushContextEvent(gl::Loop::Event::SwapChainDeprecated);
			frame.invalidate();
		});
	} else if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
		return true;
	} else {
		log::vtext("VK-Error", "Fail to vkQueuePresentKHR: ", result);
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.invalidate();
		});
	}

	return false;
}

RenderPassHandle::Sync RenderPassHandle::makeSyncInfo() {
	Sync sync;
	for (auto &it : _attachments) {
		if (it->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
			if (auto d = it.cast<SwapchainAttachmentHandle>()) {
				if (it->getAttachment()->getFirstRenderPass() == _data) {
					sync.waitAttachment.emplace_back(it);
					sync.waitSem.emplace_back(d->getSync()->getImageReady()->getSemaphore());
					sync.waitStages.emplace_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
					d->getSync()->getImageReady()->setSignaled(false);
				}

				if (it->getAttachment()->getLastRenderPass() == _data) {
					sync.signalSem.emplace_back(d->getSync()->getRenderFinished()->getUnsignalled());
					sync.signalAttachment.emplace_back(it);
					d->getSync()->getRenderFinished()->setSignaled(true);
					sync.swapchainSync.emplace_back(d->acquireSync());
				}
			}
		}
	}

	return sync;
}

}
