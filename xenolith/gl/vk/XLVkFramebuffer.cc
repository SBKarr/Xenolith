/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkFramebuffer.h"

namespace stappler::xenolith::vk {

bool Image::init(Device &dev, VkImage image, const gl::ImageInfo &info) {
	_info = info;
	_image = image;

	return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		// do nothing
	}, gl::ObjectType::ImageView, _image);
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
		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool ImageView::init(Device &dev, const gl::ImageAttachmentDescriptor &desc, Image *image) {
	VkImageViewCreateInfo createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();

	switch (desc.getInfo().imageType) {
	case gl::ImageType::Image1D: createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D; break;
	case gl::ImageType::Image2D: createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; break;
	case gl::ImageType::Image3D: createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D; break;
	}

	createInfo.format = VkFormat(desc.getInfo().format);

	bool usedAsInput = false;
	for (auto &it : desc.getRefs()) {
		if ((it->getUsage() & gl::AttachmentUsage::Input) != gl::AttachmentUsage::None) {
			usedAsInput = true;
			break;
		}
	}

	if (usedAsInput) {
		// input attachment cannot have swizzle mask
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	} else {
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
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

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView) == VK_SUCCESS) {
		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr, nullptr);
		}, gl::ObjectType::ImageView, _imageView);
	}
	return false;
}

bool Framebuffer::init(Device &dev, VkRenderPass renderPass, const Vector<VkImageView> &imageViews, Extent2 extent) {
	VkFramebufferCreateInfo framebufferInfo { };
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = imageViews.size();
	framebufferInfo.pAttachments = imageViews.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	if (dev.getTable()->vkCreateFramebuffer(dev.getDevice(), &framebufferInfo, nullptr, &_framebuffer) == VK_SUCCESS) {
		_extent = extent;
		return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyFramebuffer(d->getDevice(), (VkFramebuffer)ptr, nullptr);
		}, gl::ObjectType::Framebuffer, _framebuffer);
	}
	return false;
}

}
