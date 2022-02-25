/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkSwapchainSync.h"
#include "XLVkSync.h"
#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

SwapchainSync::~SwapchainSync() {
	if (_handle) {
		_handle->releaseUsage();
		_handle = nullptr;
	}
}

bool SwapchainSync::init(Device &dev, const Rc<SwapchainHandle> &swapchain, Mutex *mutex, uint64_t gen,
		Function<void(Rc<SwapchainSync> &&)> &&cb) {
	_imageReady = Rc<Semaphore>::create(dev);
	_renderFinished = Rc<Semaphore>::create(dev);
	_mutex = mutex;
	_handle = swapchain;
	_gen = gen;
	_cleanup = move(cb);
	return true;
}

void SwapchainSync::reset(Device &dev, const Rc<SwapchainHandle> &swapchain, uint64_t gen) {
	if (gen != _gen) {
		_imageIndex = maxOf<uint32_t>();
		if (!_imageReady->reset()) {
			_imageReady = Rc<Semaphore>::create(dev);
		}
	} else {
		if (_imageIndex == maxOf<uint32_t>()) {
			if (!_imageReady->reset()) {
				_imageReady = Rc<Semaphore>::create(dev);
			}
		}
	}
	if (!_renderFinished->reset()) {
		_renderFinished = Rc<Semaphore>::create(dev);
	}
	_gen = gen;
	_handle = swapchain;
	_waitForSubmission = false;
	_waitForPresentation = false;
}

void SwapchainSync::invalidate() {
	_imageReady = nullptr;
	_renderFinished = nullptr;
}

VkResult SwapchainSync::acquireImage(Device &dev, bool sync) {
	VkResult result = VK_ERROR_UNKNOWN;
	/*std::unique_lock<Mutex> lock(*_mutex, std::try_to_lock_t());
	if (!lock.owns_lock()) {
		return VK_ERROR_UNKNOWN;
	}*/

	if (_imageIndex != maxOf<uint32_t>()) {
		return VK_SUCCESS;
	}

	if (!_imageReady) {
		_imageReady = Rc<Semaphore>::create(dev);
	}
	dev.makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		result = table.vkAcquireNextImageKHR(device, _handle->getSwapchain(),
					sync ? maxOf<uint64_t>() : 0, _imageReady->getSemaphore(), VK_NULL_HANDLE, &_imageIndex);
	});

	log::vtext("SwapchainSync", "vkAcquireNextImageKHR: ", result, ", ", _imageIndex);

	switch (result) {
	case VK_SUCCESS:
		_imageReady->setSignaled(true);
		break;
	case VK_SUBOPTIMAL_KHR:
		_imageReady->setSignaled(true);
		_handle->deprecate();
		break;
	case VK_NOT_READY:
	case VK_TIMEOUT:
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		if (!_handle->deprecate()) {
			result = VK_ERROR_UNKNOWN;
		}
		releaseForSwapchain();
		break;
	default:
		releaseForSwapchain();
		break;
	}
	return result;
}

VkResult SwapchainSync::present(Device &dev, DeviceQueue &queue) {
	std::unique_lock<Mutex> lock(*_mutex, std::try_to_lock_t());
	if (!lock.owns_lock()) {
		return VK_ERROR_UNKNOWN;
	}

	auto result = doPresent(dev, queue);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		releaseForSwapchain();
	}
	return result;
}

VkResult SwapchainSync::submitWithPresent(Device &dev, DeviceQueue &queue, gl::FrameSync &sync,
		Fence &fence, SpanView<VkCommandBuffer> buffers, gl::PipelineStage waitStage) {
	sync.waitAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, _imageReady, waitStage});
	sync.signalAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, _renderFinished});

	/*std::unique_lock<Mutex> lock(*_mutex, std::try_to_lock_t());
	if (!lock.owns_lock()) {
		return VK_ERROR_UNKNOWN;
	}*/

	if (!queue.submit(sync, fence, buffers)) {
		releaseForSwapchain();
		return VK_ERROR_UNKNOWN;
	}

	_waitForSubmission = true;
	fence.addRelease([this, fence = &fence] (bool success) {
		setSubmitCompleted(true);
	}, this, "SwapchainSync::submitWithPresent");

	auto ret = doPresent(dev, queue);
	if (ret == VK_ERROR_OUT_OF_DATE_KHR || _handle->isDeprecated()) {
		queue.waitIdle();
	}
	return ret;
}

void SwapchainSync::clearImageIndex() {
	_imageIndex = maxOf<uint32_t>();
}

Rc<Image> SwapchainSync::getImage() const {
	return _handle->getImage(_imageIndex);
}

void SwapchainSync::setSubmitCompleted(bool val) {
	if (_waitForSubmission && val) {
		_waitForSubmission = false;
		if (_handle) {
			_handle->releaseUsage();
			_handle = nullptr;
		}
		if (!_waitForSubmission && !_waitForPresentation) {
			releaseForSwapchain();
		}
	}
}

void SwapchainSync::setPresentationEnded(bool val) {
	if (_waitForPresentation && val) {
		_waitForPresentation = false;
		if (!_waitForSubmission && !_waitForPresentation) {
			releaseForSwapchain();
		}
	}
}

VkResult SwapchainSync::doPresent(Device &dev, DeviceQueue &queue) {
	auto presentSem = _renderFinished->getSemaphore();

	VkSwapchainKHR swapchain = _handle->getSwapchain();

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &presentSem;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &_imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = VK_ERROR_UNKNOWN;
	dev.makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		log::vtext("Vk-Queue-Present", (uintptr_t)queue.getQueue());
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
	});

	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
		_imageIndex = maxOf<uint32_t>();
		_renderFinished->setWaited(true);
		_waitForPresentation = true;
		if (result == VK_SUBOPTIMAL_KHR) {
			_handle->deprecate();
		}
	} else if (result != VK_ERROR_OUT_OF_DATE_KHR) {
		log::vtext("VK-Error", "Fail to vkQueuePresentKHR: ", result);
	} else {
		if (!_handle->deprecate()) {
			result = VK_ERROR_UNKNOWN;
		}
	}
	return result;
}

void SwapchainSync::releaseForSwapchain() {
	if (_handle) {
		_handle->releaseUsage();
		_handle = nullptr;
	}
	if (_cleanup) {
		_cleanup(this);
	}
}

}
