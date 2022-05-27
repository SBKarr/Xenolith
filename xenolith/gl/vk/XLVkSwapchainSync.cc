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
		Function<void(Rc<SwapchainSync> &&)> &&releaseCb, Function<void(Rc<SwapchainSync> &&)> &&presentCb) {
	if (!gl::SwapchainImage::init(dev)) {
		return false;
	}
	_mutex = mutex;
	_handle = swapchain;
	_gen = gen;
	_cleanup = move(releaseCb);
	_present = move(presentCb);
	return true;
}

void SwapchainSync::disable() {
	if (_handle) {
		_handle->releaseUsage();
		_handle = nullptr;
	}
}

void SwapchainSync::reset(Device &dev, const Rc<SwapchainHandle> &swapchain, uint64_t gen) {
	if (gen != _gen) {
		setImage(nullptr);
		_imageIndex = maxOf<uint32_t>();
		if (!_imageReady->reset()) {
			_imageReady = dev.makeSemaphore();
		}
	} else {
		if (_imageIndex == maxOf<uint32_t>()) {
			if (!_imageReady->reset()) {
				_imageReady = dev.makeSemaphore();
			}
		}
	}
	if (!_renderFinished->reset()) {
		_renderFinished = dev.makeSemaphore();
	}
	_gen = gen;
	_handle = swapchain;
	_presented = false;
	_waitForSubmission = false;
	_waitForPresentation = false;
}

void SwapchainSync::invalidate() {
	_imageReady = nullptr;
	_renderFinished = nullptr;
}

void SwapchainSync::cleanup() {
	if (_image && _image->layout == gl::AttachmentLayout::Undefined) {
		releaseForSwapchain();
	}
}

VkResult SwapchainSync::acquireImage(Device &dev, bool sync) {
	VkResult result = VK_ERROR_UNKNOWN;

	if (_imageIndex != maxOf<uint32_t>()) {
		return VK_SUCCESS;
	}

	if (!_imageReady) {
		_imageReady = dev.makeSemaphore();
	}
	dev.makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#ifdef XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		result = table.vkAcquireNextImageKHR(device, _handle->getSwapchain(),
					sync ? maxOf<uint64_t>() : 1000000, VkSemaphore(_imageReady->getObject()), VK_NULL_HANDLE, &_imageIndex);
		XL_VKAPI_LOG("vkAcquireNextImageKHR: ", _imageIndex, " ", result,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		result = table.vkAcquireNextImageKHR(device, _handle->getSwapchain(),
					sync ? maxOf<uint64_t>() : 1000000, VkSemaphore(_imageReady->getObject()), VK_NULL_HANDLE, &_imageIndex);
#endif
	});

	switch (result) {
	case VK_SUCCESS:
		_imageReady->setSignaled(true);
		setImage(_handle->getImage(_imageIndex));
		// log::vtext("Swapchain", "vkAcquireNextImageKHR: ", _imageIndex, " VK_SUCCESS");
		break;
	case VK_SUBOPTIMAL_KHR:
		_imageReady->setSignaled(true);
		setImage(_handle->getImage(_imageIndex));
		_handle->deprecate();
		// log::vtext("Swapchain", "vkAcquireNextImageKHR: ", _imageIndex, " VK_SUBOPTIMAL_KHR");
		break;
	case VK_NOT_READY:
	case VK_TIMEOUT:
		log::vtext("Swapchain", "vkAcquireNextImageKHR: ", -1, " VK_NOT_READY");
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

VkResult SwapchainSync::submit(Device &dev, DeviceQueue &queue, gl::FrameSync &sync,
		Fence &fence, SpanView<VkCommandBuffer> buffers, gl::PipelineStage waitStage) {
	bool addImageReady = true;
	bool addRenderFinished = true;

	for (auto &it : sync.waitAttachments) {
		if (it.semaphore == _imageReady) {
			addImageReady = false;
			break;
		}
	}

	for (auto &it : sync.signalAttachments) {
		if (it.semaphore == _renderFinished) {
			addRenderFinished = false;
			break;
		}
	}

	if (addImageReady && !_imageReady->isWaited()) {
		sync.waitAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, _imageReady, waitStage});
	}
	if (addRenderFinished) {
		sync.signalAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, _renderFinished});
	}

	if (!queue.submit(sync, fence, buffers)) {
		_result = queue.getResult();
		releaseForSwapchain();
		return _result;
	}

	_result = queue.getResult();
	_waitForSubmission = true;
	fence.addRelease([this] (bool success) {
		setSubmitCompleted(true);
	}, this, "SwapchainSync::submitWithPresent");
	return _result;
}

VkResult SwapchainSync::present(Device &dev, DeviceQueue &queue) {
	_result = doPresent(dev, queue);
	if (_result == VK_ERROR_OUT_OF_DATE_KHR || _handle->isDeprecated()) {
		queue.waitIdle();
	}
	if (_result == VK_SUCCESS || _result == VK_SUBOPTIMAL_KHR) {
		if (_present) {
			_present(this);
		}
		_presented = true;
	} else {
		releaseForSwapchain();
	}
	return _result;
}

void SwapchainSync::clearImageIndex() {
	_imageIndex = maxOf<uint32_t>();
	setImage(nullptr);
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

Extent3 SwapchainSync::getImageExtent() const {
	if (_handle) {
		return _handle->getImageInfo().extent;
	}
	return Extent3(0, 0, 0);
}

VkResult SwapchainSync::doPresent(Device &dev, DeviceQueue &queue) {
	auto presentSem = VkSemaphore(_renderFinished->getObject());

	auto imageIndex = _imageIndex;

	VkResult result = _handle->present(dev, queue, presentSem, imageIndex);

	clearImageIndex();

	// log::vtext("Swapchain", "vkQueuePresentKHR: ", imageIndex);

	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
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
		log::vtext("Vk-SwapchainSync", "releaseForSwapchain: ", (void *)this);
		_handle->releaseUsage();
		_handle = nullptr;
	}
	if (_cleanup) {
		_cleanup(this);
	}
}

}
