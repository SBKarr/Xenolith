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
		d->getTable()->vkFreeMemory(d->getDevice(), (VkDeviceMemory)ptr, nullptr);
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
	VkImageViewCreateInfo createInfo = { };
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
	gl::ImageViewInfo info(image->getInfo(), desc.getColorMode());

	VkImageViewCreateInfo createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();
	createInfo.viewType = VkImageViewType(info.type);
	createInfo.format = VkFormat(info.format);

	bool usedAsInput = false;
	for (auto &it : desc.getRefs()) {
		if ((it->getUsage() & gl::AttachmentUsage::Input) != gl::AttachmentUsage::None) {
			usedAsInput = true;
			break;
		}
	}

	if (usedAsInput) {
		// input attachment cannot have swizzle mask

		if (_info.r != gl::ComponentMapping::Identity) {
			_info.r = gl::ComponentMapping::Identity;
			log::vtext("vk::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (_info.g != gl::ComponentMapping::Identity) {
			_info.g = gl::ComponentMapping::Identity;
			log::vtext("vk::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (_info.b != gl::ComponentMapping::Identity) {
			_info.b = gl::ComponentMapping::Identity;
			log::vtext("vk::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (_info.a != gl::ComponentMapping::Identity) {
			_info.a = gl::ComponentMapping::Identity;
			log::vtext("vk::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	} else {
		createInfo.components.r = VkComponentSwizzle(_info.r);
		createInfo.components.g = VkComponentSwizzle(_info.g);
		createInfo.components.b = VkComponentSwizzle(_info.b);
		createInfo.components.a = VkComponentSwizzle(_info.a);
	}

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
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = desc.getInfo().arrayLayers.get();

	_info.baseArrayLayer = gl::BaseArrayLayer(createInfo.subresourceRange.baseArrayLayer);
	_info.layerCount = gl::ArrayLayers(createInfo.subresourceRange.layerCount);

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		_image = image;
		return gl::ImageView::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool ImageView::init(Device &dev, Image *image, const gl::ImageViewInfo &info) {
	VkImageViewCreateInfo createInfo = { };
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

	_info.baseArrayLayer = gl::BaseArrayLayer(createInfo.subresourceRange.baseArrayLayer);
	_info.layerCount = gl::ArrayLayers(createInfo.subresourceRange.layerCount);

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		_info = info;
		_image = image;
		return gl::ImageView::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool Sampler::init(Device &dev, const gl::SamplerInfo &info) {
	VkSamplerCreateInfo createInfo;
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

}
