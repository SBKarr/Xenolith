/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformAndroid.h"

#if ANDROID

namespace stappler::xenolith::platform::graphic {

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() { }

bool ViewImpl::init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	auto &data = loop.getApplication()->getData();
	info.density = data.density;
	info.rect.width = data.screenSize.width * data.density;
	info.rect.height = data.screenSize.height * data.density;
	info.frameInterval = 1'000'000 / 60;

	if (!View::init(static_cast<vk::Loop &>(loop), static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	_options.presentImmediate = true;
	_options.acquireImageImmediately = true;
	return true;
}

void ViewImpl::run() {
	((NativeActivity *)_loop->getApplication()->getNativeHandle())->setView(this);
}

void ViewImpl::threadInit() {
	_started = true;
	View::threadInit();
}

void ViewImpl::mapWindow() {
	View::mapWindow();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}
	_surface = nullptr;
	_started = false;
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::update(bool displayLink) {
	if (displayLink) {
        if (_initImage) {
            presentImmediate(move(_initImage), nullptr);
            _initImage = nullptr;
            View::update(false);
        	//scheduleNextImage(_frameInterval, false);
            return;
        }

		while (_scheduledPresent.empty()) {
			View::update(false);
		}
		View::update(true);
	} else {
		View::update(displayLink);
	}
}

void ViewImpl::wakeup() {
	((NativeActivity *)_loop->getApplication()->getNativeHandle())->wakeup();
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::cancelTextInput() {

}

void ViewImpl::runWithWindow(ANativeWindow *window) {
	auto instance = _instance.cast<vk::Instance>();

	VkSurfaceKHR targetSurface;
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.window = window;

	_constraints.extent = Extent2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	if (instance->vkCreateAndroidSurfaceKHR(instance->getInstance(), &surfaceCreateInfo, nullptr, &targetSurface) != VK_SUCCESS) {
		log::text("ViewImpl", "fail to create surface");
		return;
	}

	_surface = Rc<vk::Surface>::create(instance, targetSurface);
	_nativeWindow = window;
	ANativeWindow_acquire(_nativeWindow);

	if (!_started) {
		_options.followDisplayLink = true;
		threadInit();
		_options.followDisplayLink = false;
	} else {
		initWindow();
	}
}

void ViewImpl::initWindow() {
	auto info = getSurfaceOptions();
	auto cfg = _selectConfig(info);

	createSwapchain(move(cfg), cfg.presentMode);

	if (_initImage && !_options.followDisplayLink) {
		presentImmediate(move(_initImage), nullptr);
		_initImage = nullptr;
	}

	mapWindow();
}

void ViewImpl::stopWindow() {
	clearImages();

	if (_swapchain) {
		_swapchain->invalidate();
		_swapchain = nullptr;
	}
	_surface = nullptr;

	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}
}

bool ViewImpl::pollInput(bool frameReady) {
	if (!_nativeWindow) {
		return false;
	}

	return true;
}

}

#endif
