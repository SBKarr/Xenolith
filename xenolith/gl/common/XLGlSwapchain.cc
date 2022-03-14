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

#include "XLGlSwapchain.h"

#include "XLGlFrameHandle.h"

namespace stappler::xenolith::gl {

SwapchainImage::~SwapchainImage() {
	setImage(nullptr);
}

bool SwapchainImage::init(Device &dev) {
	_imageReady = dev.makeSemaphore();
	_renderFinished = dev.makeSemaphore();
	return true;
}

void SwapchainImage::cleanup() {

}

void SwapchainImage::setImage(const Rc<gl::ImageAttachmentObject> &image) {
	if (_image == image) {
		return;
	}
	if (_image) {
		_image->waitSem = nullptr;
		_image->signalSem = nullptr;
		_image->swapchainImage = nullptr;
	}
	_image = image;
	if (_image) {
		// semaphores will be inverted on rearm call
		_image->waitSem = _imageReady;
		_image->signalSem = _renderFinished;
		_image->swapchainImage = this;
		_image->layout = gl::AttachmentLayout::Undefined;
	}
}

Swapchain::PresentTask::~PresentTask() {
	cache->releaseImage(attachment, move(object));
}

Swapchain::~Swapchain() { }

bool Swapchain::init(View *view) {
	_view = view;
	return true;
}

bool Swapchain::recreateSwapchain(Device &, gl::SwapchanCreationMode) {
	return false;
}

void Swapchain::invalidate(Device &) {

}

bool Swapchain::present(gl::Loop &, const Rc<PresentTask> &) {
	return false;
}

void Swapchain::deprecate() { }

bool Swapchain::isResetRequired() {
	return false;
}

gl::ImageInfo Swapchain::getSwapchainImageInfo() const {
	return getSwapchainImageInfo(_config);
}

gl::ImageInfo Swapchain::getSwapchainImageInfo(const gl::SwapchainConfig &cfg) const {
	gl::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = gl::ImageFlags::None;
	swapchainImageInfo.imageType = gl::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = gl::ArrayLayers( 1 );
	swapchainImageInfo.usage = gl::ImageUsage::ColorAttachment;
	if (cfg.transfer) {
		swapchainImageInfo.usage |= gl::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

gl::ImageViewInfo Swapchain::getSwapchainImageViewInfo(const gl::ImageInfo &image) const {
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

Rc<gl::ImageAttachmentObject> Swapchain::acquireImage(const Loop &loop, const ImageAttachment *a, Extent3 e) {
	return nullptr;
}

}
