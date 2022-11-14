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

#include "XLPlatformMac.h"

#if MACOS

namespace stappler::xenolith::platform::graphic {

ViewImpl::ViewImpl() {

}

ViewImpl::~ViewImpl() {

}

bool ViewImpl::init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info, float layerDensity) {
	_rect = info.rect;
	_name = info.name;

	_density = loop.getApplication()->getData().density * layerDensity;

	if (!View::init(static_cast<vk::Loop &>(loop), static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	_screenExtent = Extent2(info.rect.width * layerDensity, info.rect.height * layerDensity);

	return true;
}

void ViewImpl::run() {
	ViewImpl_run(this);
}

void ViewImpl::threadInit() {
	auto instance = _instance.cast<vk::Instance>();

	VkSurfaceKHR targetSurface;
	VkMetalSurfaceCreateInfoEXT surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.pLayer = ViewImpl_getLayer(_osView);

	if (instance->vkCreateMetalSurfaceEXT(instance->getInstance(), &surfaceCreateInfo, nullptr, &targetSurface) != VK_SUCCESS) {
		log::text("ViewImpl", "fail to create surface");
		return;
	}

	_surface = Rc<vk::Surface>::create(instance, targetSurface);

	vk::View::threadDispose();
}

void ViewImpl::threadDispose() {
	vk::View::threadDispose();
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::wakeup() {
	ViewImpl_wakeup(this);
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::cancelTextInput() {

}

void ViewImpl::presentWithQueue(vk::DeviceQueue &, Rc<ImageStorage> &&) {

}

void ViewImpl::mapWindow() {

}

bool ViewImpl::pollInput(bool frameReady) {
	return false;
}

gl::SurfaceInfo ViewImpl::getSurfaceOptions() const {
	auto opts = vk::View::getSurfaceOptions();
	opts.surfaceDensity = ViewImpl_getSurfaceDensity(_osView);
	return opts;
}

void ViewImpl::finalize() {

}

gl::ImageFormat getCommonFormat() {
	return gl::ImageFormat::B8G8R8A8_UNORM;
}

Rc<gl::View> createView(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, dev, move(info), ViewImpl_getScreenDensity());
}

}

#endif
