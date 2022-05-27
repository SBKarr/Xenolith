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

#include "XLVkObject.h"

namespace stappler::xenolith::vk {

bool DeviceMemory::init(Device &dev, VkDeviceMemory memory) {
	_memory = memory;

	return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
			table.vkFreeMemory(device, (VkDeviceMemory)ptr, nullptr);
		});
	}, gl::ObjectType::DeviceMemory, _memory);
}

bool Image::init(Device &dev, VkImage image, const gl::ImageInfo &info, Rc<gl::ImageAtlas> &&atlas) {
	_info = info;
	_image = image;
	_atlas = atlas;

	return gl::ImageObject::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		// do nothing
	}, gl::ObjectType::Image, _image);
}

bool Image::init(Device &dev, VkImage image, const gl::ImageInfo &info, Rc<DeviceMemory> &&mem, Rc<gl::ImageAtlas> &&atlas) {
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return gl::ImageObject::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr, nullptr);
	}, gl::ObjectType::Image, _image);
}

bool Image::init(Device &dev, uint64_t idx, VkImage image, const gl::ImageInfo &info, Rc<DeviceMemory> &&mem, Rc<gl::ImageAtlas> &&atlas) {
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return gl::ImageObject::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr, nullptr);
	}, gl::ObjectType::Image, _image, idx);
}

void Image::setPendingBarrier(const VkImageMemoryBarrier &barrier) {
	_barrier = barrier;
}

const VkImageMemoryBarrier *Image::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Image::dropPendingBarrier() {
	_barrier.reset();
}

bool Buffer::init(Device &dev, VkBuffer buffer, const gl::BufferInfo &info, Rc<DeviceMemory> &&mem) {
	_info = info;
	_buffer = buffer;
	_memory = move(mem);

	return gl::BufferObject::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyBuffer(d->getDevice(), (VkBuffer)ptr, nullptr);
	}, gl::ObjectType::Buffer, _buffer);
}

void Buffer::setPendingBarrier(const VkBufferMemoryBarrier &barrier) {
	_barrier = barrier;
}

const VkBufferMemoryBarrier *Buffer::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Buffer::dropPendingBarrier() {
	_barrier.reset();
}

bool ImageView::init(Device &dev, VkImage image, VkFormat format) {
	VkImageViewCreateInfo createInfo{}; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		return gl::ImageView::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool ImageView::init(Device &dev, const gl::ImageAttachmentDescriptor &desc, Image *image) {
	gl::ImageViewInfo info(desc);

	VkImageViewCreateInfo createInfo{};  sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();
	createInfo.viewType = VkImageViewType(info.type);
	createInfo.format = VkFormat(info.format);

	auto ops = desc.getOps();

	switch (desc.getInfo().format) {
	case gl::ImageFormat::D16_UNORM:
	case gl::ImageFormat::X8_D24_UNORM_PACK32:
	case gl::ImageFormat::D32_SFLOAT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case gl::ImageFormat::S8_UINT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	case gl::ImageFormat::D16_UNORM_S8_UINT:
	case gl::ImageFormat::D24_UNORM_S8_UINT:
	case gl::ImageFormat::D32_SFLOAT_S8_UINT:
		createInfo.subresourceRange.aspectMask = 0;
		if ((ops & gl::AttachmentOps::ReadStencil) != gl::AttachmentOps::Undefined
				|| (ops & gl::AttachmentOps::WritesStencil) != gl::AttachmentOps::Undefined) {
			createInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		if ((ops & gl::AttachmentOps::ReadColor) != gl::AttachmentOps::Undefined
				|| (ops & gl::AttachmentOps::WritesColor) != gl::AttachmentOps::Undefined) {
			createInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		break;
	default:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	}

	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = desc.getInfo().mipLevels.get();
	createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer.get();
	createInfo.subresourceRange.layerCount = info.layerCount.get();

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		_info = move(info);
		_image = image;
		return gl::ImageView::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool ImageView::init(Device &dev, Image *image, const gl::ImageViewInfo &info) {
	VkImageViewCreateInfo createInfo{}; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();

	switch (info.type) {
	case gl::ImageViewType::ImageView1D:
		if (image->getInfo().imageType != gl::ImageType::Image1D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case gl::ImageViewType::ImageView1DArray:
		if (image->getInfo().imageType != gl::ImageType::Image1D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case gl::ImageViewType::ImageView2D:
		if (image->getInfo().imageType != gl::ImageType::Image2D && image->getInfo().imageType != gl::ImageType::Image3D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case gl::ImageViewType::ImageView2DArray:
		if (image->getInfo().imageType != gl::ImageType::Image2D && image->getInfo().imageType != gl::ImageType::Image3D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case gl::ImageViewType::ImageView3D:
		if (image->getInfo().imageType != gl::ImageType::Image3D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case gl::ImageViewType::ImageViewCube:
		if (image->getInfo().imageType != gl::ImageType::Image2D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case gl::ImageViewType::ImageViewCubeArray:
		if (image->getInfo().imageType != gl::ImageType::Image2D) {
			log::vtext("Vk-ImageView", "Incompatible ImageType '", gl::getImageTypeName(image->getInfo().imageType),
					"' and ImageViewType '", gl::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	}

	auto format = info.format;
	if (format == gl::ImageFormat::Undefined) {
		format = image->getInfo().format;
	}
	createInfo.format = VkFormat(format);

	createInfo.components.r = VkComponentSwizzle(info.r);
	createInfo.components.g = VkComponentSwizzle(info.g);
	createInfo.components.b = VkComponentSwizzle(info.b);
	createInfo.components.a = VkComponentSwizzle(info.a);

	switch (format) {
	case gl::ImageFormat::D16_UNORM:
	case gl::ImageFormat::X8_D24_UNORM_PACK32:
	case gl::ImageFormat::D32_SFLOAT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case gl::ImageFormat::S8_UINT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	case gl::ImageFormat::D16_UNORM_S8_UINT:
	case gl::ImageFormat::D24_UNORM_S8_UINT:
	case gl::ImageFormat::D32_SFLOAT_S8_UINT:
		createInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	default:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	}

	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = image->getInfo().mipLevels.get();
	createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer.get();
	if (info.layerCount.get() == maxOf<uint32_t>()) {
		createInfo.subresourceRange.layerCount = image->getInfo().arrayLayers.get() - info.baseArrayLayer.get();
	} else {
		createInfo.subresourceRange.layerCount = info.layerCount.get();
	}

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		_info = info;
		_info.format = format;
		_info.baseArrayLayer = gl::BaseArrayLayer(createInfo.subresourceRange.baseArrayLayer);
		_info.layerCount = gl::ArrayLayers(createInfo.subresourceRange.layerCount);

		_image = image;
		return gl::ImageView::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool Sampler::init(Device &dev, const gl::SamplerInfo &info) {
	VkSamplerCreateInfo createInfo; sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = VkFilter(info.magFilter);
	createInfo.minFilter = VkFilter(info.minFilter);
	createInfo.mipmapMode = VkSamplerMipmapMode(info.mipmapMode);
	createInfo.addressModeU = VkSamplerAddressMode(info.addressModeU);
	createInfo.addressModeV = VkSamplerAddressMode(info.addressModeV);
	createInfo.addressModeW = VkSamplerAddressMode(info.addressModeW);
	createInfo.mipLodBias = info.mipLodBias;
	createInfo.anisotropyEnable = info.anisotropyEnable;
	createInfo.maxAnisotropy = info.maxAnisotropy;
	createInfo.compareEnable = info.compareEnable;
	createInfo.compareOp = VkCompareOp(info.compareOp);
	createInfo.minLod = info.minLod;
	createInfo.maxLod = info.maxLod;
	createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	createInfo.unnormalizedCoordinates = false;

	if (dev.getTable()->vkCreateSampler(dev.getDevice(), &createInfo, nullptr, &_sampler) == VK_SUCCESS) {
		_info = info;
		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroySampler(d->getDevice(), (VkSampler)ptr, nullptr);
		}, gl::ObjectType::Sampler, _sampler);
	}
	return false;
}

bool SwapchainHandle::init(Device &dev, const gl::SwapchainConfig &cfg, gl::ImageInfo &&swapchainImageInfo,
		gl::PresentMode presentMode, VkSurfaceKHR surface, uint32_t families[2], Function<void(gl::SwapchanCreationMode)> &&cb, SwapchainHandle *old) {
	VkSwapchainCreateInfoKHR swapChainCreateInfo{}; sanitizeVkStruct(swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
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
		result = table.vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &_swapchain);
	});
	if (result == VK_SUCCESS) {
		Vector<VkImage> swapchainImages;

		uint32_t imageCount = 0;
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, nullptr);
		swapchainImages.resize(imageCount);
		dev.getTable()->vkGetSwapchainImagesKHR(dev.getDevice(), _swapchain, &imageCount, swapchainImages.data());

		_images.reserve(imageCount);
		for (auto &it : swapchainImages) {
			auto image = Rc<Image>::create(dev, it, swapchainImageInfo);
			auto obj = Rc<gl::ImageAttachmentObject>::alloc();
			obj->extent = swapchainImageInfo.extent;
			obj->image = image.get();
			obj->isSwapchainImage = true;

			_images.emplace_back(move(obj));
		}

		_presentMode = presentMode;
		_imageInfo = move(swapchainImageInfo);
		_config = move(cfg);
		_rebuildCallback = move(cb);

		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
				table.vkDestroySwapchainKHR(device, (VkSwapchainKHR)ptr, nullptr);
			});
		}, gl::ObjectType::Swapchain, _swapchain);
	}
	return false;
}

bool SwapchainHandle::isDeprecated() {
	std::unique_lock<Mutex> lock(_mutex);
	return _deprecated;
}

bool SwapchainHandle::isOptimal() const {
	return _presentMode == _config.presentMode;
}

Rc<gl::ImageAttachmentObject> SwapchainHandle::getImage(uint32_t i) const {
	if (i < _images.size()) {
		return _images[i];
	}
	return nullptr;
}

bool SwapchainHandle::deprecate(bool invalidate) {
	std::unique_lock<Mutex> lock(_mutex);

	auto tmp = _deprecated;
	_deprecated = true;
	if (!invalidate) {
		if (!tmp && _inUse == 0 && _rebuildCallback) {
			_rebuildCallback(_rebuildMode);
			_rebuildCallback = nullptr;
		}
	} else {
		_rebuildCallback = nullptr;
	}
	return !tmp;
}

bool SwapchainHandle::retainUsage() {
	std::unique_lock<Mutex> lock(_mutex);

	if (!_deprecated) {
		++ _inUse;
		return true;
	}
	return false;
}

void SwapchainHandle::releaseUsage() {
	std::unique_lock<Mutex> lock(_mutex);

	-- _inUse;
	if (_inUse == 0 && _deprecated && _rebuildCallback) {
		_rebuildCallback(_rebuildMode);
		_rebuildCallback = nullptr;
	}
}

VkResult SwapchainHandle::present(Device &dev, DeviceQueue &queue, VkSemaphore presentSem, uint32_t imageIdx) {
	VkPresentInfoKHR presentInfo{}; sanitizeVkStruct(presentInfo);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &presentSem;

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.pImageIndices = &imageIdx;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = VK_ERROR_UNKNOWN;
	dev.makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#ifdef XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
		XL_VKAPI_LOG("[", queue.getFrameIndex(), "] vkQueuePresentKHR: ", imageIdx, " ", result,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		result = table.vkQueuePresentKHR(queue.getQueue(), &presentInfo);
#endif
	});

	if (result == VK_SUCCESS) {
		auto ret = _presentedFrames.fetch_add(1);
		if (ret >= config::MaxSuboptimalFrame && _presentMode == _config.presentModeFast
				&& _config.presentModeFast != _config.presentMode) {
			result = VK_SUBOPTIMAL_KHR;
			_rebuildMode = gl::SwapchanCreationMode::Best;
		}
	}

	return result;
}


}
