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

#include "XLDirector.h"
#include "XLTextInputManager.h"

namespace stappler::xenolith::platform::graphic {

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() { }

bool ViewImpl::init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info, float layerDensity) {
	_rect = info.rect;
	_name = info.name;

	_density = loop.getApplication()->getData().density * layerDensity;

	if (!View::init(static_cast<vk::Loop &>(loop), static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	_screenExtent = Extent2(info.rect.width * layerDensity, info.rect.height * layerDensity);
	_frameInterval = 0;
	//_options.followDisplayLink = true;

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

	vk::View::threadInit();
}

void ViewImpl::threadDispose() {
	vk::View::threadDispose();

	_loop->performOnGlThread([this] {
		end();
	}, this);
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::update(bool displayLink) {
	View::update(!_displayLinkFlag.test_and_set());
}

void ViewImpl::wakeup() {
	ViewImpl_wakeup(this);
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {
	performOnThread([this, pos, len] {
		ViewImpl_updateTextCursor(_osView, pos, len);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		ViewImpl_updateTextInput(_osView, str, pos, len, type);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	performOnThread([this, str = str.str<Interface>(), pos, len, type] {
		_inputEnabled = true;
		ViewImpl_runTextInput(_osView, str, pos, len, type);
		_director->getApplication()->performOnMainThread([this] () {
			_director->getTextInputManager()->setInputEnabled(true);
		}, this);
	}, this);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		_inputEnabled = false;
		ViewImpl_cancelTextInput(_osView);
		_director->getApplication()->performOnMainThread([&] () {
			_director->getTextInputManager()->setInputEnabled(false);
		}, this);
	}, this);
}

void ViewImpl::mapWindow() {

}

void ViewImpl::handleDisplayLinkCallback() {
	if (!_followDisplayLink) {
		return;
	}

	_displayLinkFlag.clear();
	wakeup();
}

void ViewImpl::startLiveResize() {
	_liveResize = true;
	_loop->performOnGlThread([this] {
		_loop->getFrameCache()->freeze();
	}, this);
	_options.renderImageOffscreen = true;
	deprecateSwapchain(false);
	/*_swapchain->deprecate(false);

	auto it = _scheduledPresent.begin();
	while (it != _scheduledPresent.end()) {
		runScheduledPresent(move(*it));
		it = _scheduledPresent.erase(it);
	}

	// log::vtext("View", "recreateSwapchain - View::deprecateSwapchain (", renderqueue::FrameHandle::GetActiveFramesCount(), ")");
	recreateSwapchain(_swapchain->getRebuildMode());*/
}

void ViewImpl::stopLiveResize() {
	_options.renderImageOffscreen = false;
	deprecateSwapchain(false);

	_loop->performOnGlThread([this] {
		_loop->getFrameCache()->unfreeze();
	}, this);
	_liveResize = false;
}

void ViewImpl::submitTextData(WideStringView str, TextCursor cursor, TextCursor marked) {
	_director->getApplication()->performOnMainThread([this, str = str.str<Interface>(), cursor, marked] {
		_director->getTextInputManager()->textChanged(str, cursor, marked);
	}, this);
}

bool ViewImpl::pollInput(bool frameReady) {
	return true; // we should return false when view should be closed
}

bool ViewImpl::createSwapchain(gl::SwapchainConfig &&cfg, gl::PresentMode presentMode) {
	_loop->performOnGlThread([this] {
		_loop->getFrameCache()->clear();
	}, this);

	if (presentMode == gl::PresentMode::Immediate) {
		_options.followDisplayLink = false;
		ViewImpl_setVSyncEnabled(_osView, false);
	} else {
		_options.followDisplayLink = true;
		ViewImpl_setVSyncEnabled(_osView, true);
	}

	return vk::View::createSwapchain(move(cfg), presentMode);
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
	auto screenDensity = ViewImpl_getScreenDensity();
	return Rc<ViewImpl>::create(loop, dev, move(info), screenDensity);
}

}

#endif
