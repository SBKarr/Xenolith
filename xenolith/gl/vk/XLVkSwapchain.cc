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

#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

Surface::~Surface() {
	if (_surface) {
		_instance->vkDestroySurfaceKHR(_instance->getInstance(), _surface, nullptr);
		_surface = VK_NULL_HANDLE;
	}
	_window = nullptr;
}

bool Surface::init(Instance *instance, VkSurfaceKHR surface, Ref *win) {
	if (!surface) {
		return false;
	}
	_instance = instance;
	_surface = surface;
	_window = win;
	return true;
}

SwapchainHandle::~SwapchainHandle() {
	for (auto &it : _images) {
		for (auto &v : it.views) {
			v.second->invalidate();
			v.second = nullptr;
		}
	}
	invalidate();
}

bool SwapchainHandle::init(Device &dev, const gl::SwapchainConfig &cfg, gl::ImageInfo &&swapchainImageInfo,
		gl::PresentMode presentMode, Surface *surface, uint32_t families[2], SwapchainHandle *old) {

	_device = &dev;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{}; sanitizeVkStruct(swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface->getSurface();
	swapChainCreateInfo.minImageCount = cfg.imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = VkColorSpaceKHR(cfg.colorSpace);
	swapChainCreateInfo.imageExtent = VkExtent2D({swapchainImageInfo.extent.width, swapchainImageInfo.extent.height});
	swapChainCreateInfo.imageArrayLayers = swapchainImageInfo.arrayLayers.get();
	swapChainCreateInfo.imageUsage = VkImageUsageFlags(swapchainImageInfo.usage);

	if (families[0] != families[1]) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = families;
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapChainCreateInfo.preTransform = VkSurfaceTransformFlagBitsKHR(cfg.transform);
	swapChainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(cfg.alpha);
	swapChainCreateInfo.presentMode = getVkPresentMode(presentMode);
	swapChainCreateInfo.clipped = (cfg.clipped ? VK_TRUE : VK_FALSE);

	if (old) {
		swapChainCreateInfo.oldSwapchain = old->getSwapchain();
	} else {
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	}

	VkResult result = VK_ERROR_UNKNOWN;
	dev.makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapchain);
		XL_VKAPI_LOG("vkCreateSwapchainKHR: ", result,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapchain);
#endif
	});
	if (result == VK_SUCCESS) {
		if (old) {
			std::unique_lock<Mutex> lock(_resourceMutex);
			std::unique_lock<Mutex> lock2(old->_resourceMutex);
			_semaphores = move(old->_semaphores);
			if (old->_presentSemaphore) {
				releaseSemaphore(move(old->_presentSemaphore));
			}
		}

		Vector<VkImage> swapchainImages;

		uint32_t imageCount = 0;
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, nullptr);
		swapchainImages.resize(imageCount);
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, swapchainImages.data());

		_images.reserve(imageCount);

		auto swapchainImageViewInfo = getSwapchainImageViewInfo(swapchainImageInfo);

		for (auto &it : swapchainImages) {
			auto image = Rc<Image>::create(dev, it, swapchainImageInfo, _images.size());

			Map<gl::ImageViewInfo, Rc<ImageView>> views;
			views.emplace(swapchainImageViewInfo, Rc<ImageView>::create(dev, image.get(), swapchainImageViewInfo));

			_images.emplace_back(SwapchainImageData{move(image), move(views)});
		}

		_rebuildMode = _presentMode = presentMode;
		_imageInfo = move(swapchainImageInfo);
		_config = move(cfg);
		_surface = surface;

		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
				auto t = platform::device::_clock(platform::device::Monotonic);
				table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)ptr, nullptr);
				XL_VKAPI_LOG("vkDestroySwapchainKHR: [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
				table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)ptr, nullptr);
#endif

			});
		}, gl::ObjectType::Swapchain, _swapchain);
	}
	return false;
}

bool SwapchainHandle::isDeprecated() {
	return _deprecated;
}

bool SwapchainHandle::isOptimal() const {
	return _presentMode == _config.presentMode;
}

bool SwapchainHandle::deprecate(bool fast) {
	std::cout << "deprecate " << fast << "\n";
	auto tmp = _deprecated;
	_deprecated = true;
	if (fast && _config.presentModeFast != gl::PresentMode::Unsupported) {
		_rebuildMode = _config.presentModeFast;
	}
	return !tmp;
}

auto SwapchainHandle::acquire(bool lockfree, const Rc<Fence> &fence) -> Rc<ImageStorage> {
	if (_deprecated) {
		return nullptr;
	}

	uint64_t timeout = lockfree ? 0 : maxOf<uint64_t>();
	Rc<Semaphore> sem = acquireSemaphore();
	uint32_t imageIndex = maxOf<uint32_t>();
	VkResult ret = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		ret = table.vkAcquireNextImageKHR(device, _swapchain, timeout,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence->getFence(), &imageIndex);
		XL_VKAPI_LOG("vkAcquireNextImageKHR: ", imageIndex, " ", ret,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		ret = table.vkAcquireNextImageKHR(device, _swapchain, timeout,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence->getFence(), &imageIndex);
#endif
	});

	Rc<SwapchainImage> image;
	switch (ret) {
	case VK_SUCCESS:
		if (sem) {
			sem->setSignaled(true);
		}
		fence->setTag("SwapchainHandle::acquire");
		fence->setArmed();
		++ _acquiredImages;
		image = Rc<SwapchainImage>::create(this, _images.at(imageIndex), move(sem));
#if XL_VKAPI_DEBUG
		image->setAcquisitionTime(platform::device::_clock(platform::device::Monotonic));
#endif
		return move(image);
		break;
	case VK_SUBOPTIMAL_KHR:
		if (sem) {
			sem->setSignaled(true);
		}
		fence->setTag("SwapchainHandle::acquire");
		fence->setArmed();
		_deprecated = true;
		++ _acquiredImages;
		image = Rc<SwapchainImage>::create(this, _images.at(imageIndex), move(sem));
#if XL_VKAPI_DEBUG
		image->setAcquisitionTime(platform::device::_clock(platform::device::Monotonic));
#endif
		return move(image);
		break;
	default:
		releaseSemaphore(move(sem));
		break;
	}

	return nullptr;
}

bool SwapchainHandle::acquire(const Rc<SwapchainImage> &image, const Rc<Fence> &fence, bool immediate) {
	if (image->getSwapchain() != this) {
		return false;
	}

	Rc<Semaphore> sem = acquireSemaphore();
	uint32_t imageIndex = maxOf<uint32_t>();
	VkResult ret = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		ret = table.vkAcquireNextImageKHR(device, _swapchain, immediate ? maxOf<uint64_t>() : 0,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence->getFence(), &imageIndex);
		XL_VKAPI_LOG("vkAcquireNextImageKHR: ", imageIndex, " ", ret,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		ret = table.vkAcquireNextImageKHR(device, _swapchain, immediate ? maxOf<uint64_t>() : 0,
				sem ? sem->getSemaphore() : VK_NULL_HANDLE, fence->getFence(), &imageIndex);
#endif
	});

	switch (ret) {
	case VK_SUCCESS:
		if (sem) {
			sem->setSignaled(true);
		}
		fence->setTag("SwapchainHandle::acquire");
		fence->setArmed();
		++ _acquiredImages;
		image->setImage(this, _images.at(imageIndex), move(sem));
#if XL_VKAPI_DEBUG
		image->setAcquisitionTime(platform::device::_clock(platform::device::Monotonic));
#endif
		return true;
		break;
	case VK_SUBOPTIMAL_KHR:
		if (sem) {
			sem->setSignaled(true);
		}
		fence->setTag("SwapchainHandle::acquire");
		fence->setArmed();
		_deprecated = true;
		++ _acquiredImages;
		image->setImage(this, _images.at(imageIndex), move(sem));
#if XL_VKAPI_DEBUG
		image->setAcquisitionTime(platform::device::_clock(platform::device::Monotonic));
#endif
		return true;
		break;
	default:
		releaseSemaphore(move(sem));
		break;
	}

	return false;
}

VkResult SwapchainHandle::present(DeviceQueue &queue, const Rc<ImageStorage> &image) {
	auto waitSem = ((Semaphore *)image->getSignalSem().get())->getSemaphore();
	auto imageIndex = image->getImageIndex();

	VkPresentInfoKHR presentInfo{}; sanitizeVkStruct(presentInfo);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &waitSem;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
		XL_VKAPI_LOG("[", image->getFrameIndex(), "] vkQueuePresentKHR: ", imageIndex, " ", result,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "] [timeout: ", t - _presentTime,
				"] [acquisition: ", t - image->getAcquisitionTime(), "]");
		_presentTime = t;
#else
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
#endif
	});

	do {
		std::unique_lock<Mutex> lock(_resourceMutex);
		((SwapchainImage *)image.get())->setPresented();
		-- _acquiredImages;
	} while (0);

	if (result == VK_SUCCESS) {
		if (_presentSemaphore) {
			_presentSemaphore->setWaited(true);
			releaseSemaphore(move(_presentSemaphore));
		}
		_presentSemaphore = (Semaphore *)image->getSignalSem().get();

		++ _presentedFrames;
		if (_presentedFrames == config::MaxSuboptimalFrame && _presentMode == _config.presentModeFast
				&& _config.presentModeFast != _config.presentMode) {
			result = VK_SUBOPTIMAL_KHR;
			_rebuildMode = _config.presentMode;
		}
	}

	return result;
}

void SwapchainHandle::invalidateImage(const ImageStorage *image) {

	if (!((SwapchainImage *)image)->isPresented()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		-- _acquiredImages;
	}
}

Rc<Semaphore> SwapchainHandle::acquireSemaphore() {
	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!_semaphores.empty()) {
		auto sem = _semaphores.back();
		_semaphores.pop_back();
		return sem;
	}
	lock.unlock();

	return Rc<Semaphore>::create(*_device);
}

void SwapchainHandle::releaseSemaphore(Rc<Semaphore> &&sem) {
	if (sem && sem->reset()) {
		std::unique_lock<Mutex> lock(_resourceMutex);
		_semaphores.emplace_back(move(sem));
	}
}

Rc<gl::ImageView> SwapchainHandle::makeView(const Rc<gl::ImageObject> &image, const gl::ImageViewInfo &viewInfo) {
	auto img = (Image *)image.get();
	auto idx = img->getIndex();

	auto it = _images[idx].views.find(viewInfo);
	if (it != _images[idx].views.end()) {
		return it->second;
	}

	it = _images[idx].views.emplace(viewInfo, Rc<ImageView>::create(*_device, img, viewInfo)).first;
	return it->second;
}

gl::ImageViewInfo SwapchainHandle::getSwapchainImageViewInfo(const gl::ImageInfo &image) const {
	gl::ImageViewInfo info;
	switch (image.imageType) {
	case gl::ImageType::Image1D:
		info.type = gl::ImageViewType::ImageView1D;
		break;
	case gl::ImageType::Image2D:
		info.type = gl::ImageViewType::ImageView2D;
		break;
	case gl::ImageType::Image3D:
		info.type = gl::ImageViewType::ImageView3D;
		break;
	}

	return image.getViewInfo(info);
}


SwapchainImage::~SwapchainImage() {
	if (_state != State::Presented) {
		if (_image) {
			_swapchain->invalidateImage(this);
		}
		_image = nullptr;
		_swapchain = nullptr;
		_state = State::Presented;
	}
}

bool SwapchainImage::init(Rc<SwapchainHandle> &&swapchain, uint64_t order, uint64_t presentWindow) {
	_swapchain = move(swapchain);
	_order = order;
	_presentWindow = presentWindow;
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

bool SwapchainImage::init(Rc<SwapchainHandle> &&swapchain, const SwapchainHandle::SwapchainImageData &image, const Rc<Semaphore> &sem) {
	_swapchain = move(swapchain);
	_image = image.image.get();
	for (auto &it : image.views) {
		_views.emplace(it.first, it.second);
	}
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
	_state = State::Submitted;
	_isSwapchainImage = true;
	return true;
}

void SwapchainImage::cleanup() {
	stappler::log::text("SwapchainImage", "cleanup");
}

void SwapchainImage::rearmSemaphores(gl::Loop &loop) {
	ImageStorage::rearmSemaphores(loop);
}

void SwapchainImage::releaseSemaphore(gl::Semaphore *sem) {
	if (_state == State::Presented && sem == _waitSem && _swapchain) {
		// work on last submit is over, wait sem no longer in use
		_swapchain->releaseSemaphore((Semaphore *)sem);
		_waitSem = nullptr;
	}
}

gl::ImageInfoData SwapchainImage::getInfo() const {
	if (_image) {
		return _image->getInfo();
	} else {
		return _swapchain->getImageInfo();
	}
}

Rc<gl::ImageView> SwapchainImage::makeView(const gl::ImageViewInfo &info) {
	auto it = _views.find(info);
	if (it != _views.end()) {
		return it->second;
	}

	it = _views.emplace(info, _swapchain->makeView(_image, info)).first;
	return it->second;
}

void SwapchainImage::setImage(Rc<SwapchainHandle> &&handle, const SwapchainHandle::SwapchainImageData &image, const Rc<Semaphore> &sem) {
	_image = image.image.get();
	for (auto &it : image.views) {
		_views.emplace(it.first, it.second);
	}
	if (sem) {
		_waitSem = sem.get();
	}
	_signalSem = _swapchain->acquireSemaphore().get();
}

void SwapchainImage::setPresented() {
	_state = State::Presented;
}

void SwapchainImage::invalidateImage() {
	if (_image) {
		_swapchain->invalidateImage(this);
	}
	_swapchain = nullptr;
	_state = State::Presented;
}

void SwapchainImage::invalidateSwapchain() {
	_swapchain = nullptr;
	_image = nullptr;
	_state = State::Presented;
}

}
