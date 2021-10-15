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

#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

ImageAttachment::~ImageAttachment() { }

void ImageAttachment::clear() {
	gl::ImageAttachment::clear();
	_image = nullptr;
}

void ImageAttachment::setImage(const Rc<Image> &img) {
	_image = img;
}

Rc<gl::AttachmentDescriptor> ImageAttachment::makeDescriptor(gl::RenderPassData *pass) {
	return Rc<ImageAttachmentDescriptor>::create(pass, this);
}

ImageAttachmentDescriptor::~ImageAttachmentDescriptor() { }

void ImageAttachmentDescriptor::clear() {
	gl::ImageAttachmentDescriptor::clear();
	_imageView = nullptr;
}

void ImageAttachmentDescriptor::setImageView(const Rc<ImageView> &img) {
	_imageView = img;
}

SwapchainAttachment::~SwapchainAttachment() { }

void SwapchainAttachment::clear() {
	gl::SwapchainAttachment::clear();
	_images.clear();
}

void SwapchainAttachment::setImages(Vector<Rc<Image>> &&images) {
	_images = move(images);
}

Rc<gl::AttachmentHandle> SwapchainAttachment::makeFrameHandle(const gl::FrameHandle &handle) {
	return Rc<SwapchainAttachmentHandle>::create(this, handle);
}

Rc<gl::AttachmentDescriptor> SwapchainAttachment::makeDescriptor(gl::RenderPassData *pass) {
	return Rc<SwapchainAttachmentDescriptor>::create(pass, this);
}

SwapchainAttachmentDescriptor::~SwapchainAttachmentDescriptor() { }

void SwapchainAttachmentDescriptor::clear() {
	gl::SwapchainAttachmentDescriptor::clear();
	_imageViews.clear();
}

void SwapchainAttachmentDescriptor::setImageViews(Vector<Rc<ImageView>> &&imageViews) {
	_imageViews = move(imageViews);
}

ImageAttachmentHandle::~ImageAttachmentHandle() {

}

SwapchainAttachmentHandle::~SwapchainAttachmentHandle() {
	invalidate();
}

bool SwapchainAttachmentHandle::isAvailable(const gl::FrameHandle &frame) const {
	auto a = (SwapchainAttachment *)_attachment.get();
	if (a->getOwner() == &frame) {
		return true;
	}
	return false;
}

bool SwapchainAttachmentHandle::setup(gl::FrameHandle &handle) {
	_device = (Device *)handle.getDevice();
	_swapchain = (Swapchain *)handle.getSwapchain();
	_sync = static_cast<FrameHandle &>(handle).acquireSwapchainSync();
	if (acquire(handle)) {
		return true;
	}
	return false;
}

Rc<SwapchainSync> SwapchainAttachmentHandle::acquireSync() {
	auto ret = move(_sync);
	_sync = nullptr;
	return ret;
}

bool SwapchainAttachmentHandle::acquire(gl::FrameHandle &handle) {
	VkResult result = _sync->acquireImage(*_device, *_swapchain, &_index);
	switch (result) {
	case VK_ERROR_OUT_OF_DATE_KHR:
		handle.getLoop()->recreateSwapChain(handle.getSwapchain());
		handle.invalidate();
		return true;
		break;
	case VK_SUCCESS: // acquired successfully
	case VK_SUBOPTIMAL_KHR: // swapchain recreation signal will be sent by view, but we can try to do some frames until that
		return true;
		break;

	case VK_NOT_READY:

		// VK_TIMEOUT result is not documented, but some drivers returns that
		// see https://community.amd.com/t5/opengl-vulkan/vkacquirenextimagekhr-with-0-timeout-returns-vk-timeout-instead/td-p/350023
	case VK_TIMEOUT:
		// schedule
		handle.schedule([this] (gl::FrameHandle &handle, gl::Loop::Context &context) {
			if (!handle.isValid()) {
				invalidate();
				return true; // end spinning
			}

			VkResult result = _sync->acquireImage(*_device, *_swapchain, &_index);
			switch (result) {
			case VK_ERROR_OUT_OF_DATE_KHR:
				// push swapchain invalidation
				XL_VK_LOG("vkAcquireNextImageKHR: VK_ERROR_OUT_OF_DATE_KHR");
				context.events->emplace_back(gl::Loop::EventName::SwapChainDeprecated, _swapchain, data::Value());
				handle.invalidate();
				invalidate();
				return true; // end spinning
				break;
			case VK_SUCCESS:
			case VK_SUBOPTIMAL_KHR:
				// acquired successfully
				handle.setAttachmentReady(this);
				return true; // end spinning
				break;
			case VK_NOT_READY:
			case VK_TIMEOUT:
				return false; // continue spinning
				break;
			default:
				invalidate();
				handle.invalidate();
				return true; // end spinning
				break;
			}
			return true;
		});
		return false;
		break;
	default:
		invalidate();
		handle.invalidate();
		break;
	}
	return false;
}

void SwapchainAttachmentHandle::invalidate() {
	if (_sync) {
		_swapchain->releaseSwapchainSync(move(_sync));
		_sync = nullptr;
	}
}

}
