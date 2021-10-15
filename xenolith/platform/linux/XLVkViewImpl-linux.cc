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

#include "XLDefine.h"

#if LINUX

#include "XLScene.h"
#include "XLGlLoop.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() {
	_vkDevice = nullptr;
	_view = nullptr;
	_vkInstance = nullptr;
}

bool ViewImpl::init(const Rc<EventLoop> &ev, const Rc<gl::Loop> &loop, StringView viewName, URect rect) {
	_vkInstance = dynamic_cast<const vk::Instance *>(loop->getInstance());
	_vkDevice = dynamic_cast<vk::Device *>(loop->getDevice().get());

	if (!_vkInstance || !_vkDevice) {
		return false;
	}

	auto v = Rc<XcbView>::alloc(_vkInstance, this, viewName, rect);
	if (!v->isAvailableOnDevice(*_vkDevice)) {
		return false;
	}

	_view = v.get();
	return gl::View::init(ev, loop);
}

bool ViewImpl::begin(const Rc<Director> &director, Function<void()> &&cb) {
	if (!_vkDevice) {
		return false;
	}

	_surface = _view->createWindowSurface();
	if (!_surface) {
		log::text("VkView", "Fail to create Vulkan surface for window");
		return false;
	}

	if (!isAvailableOnDevice(_surface)) {
		return false;
	}

	return View::begin(director, move(cb));
}

void ViewImpl::end() {
	View::end();
}

bool ViewImpl::isAvailableOnDevice(VkSurfaceKHR surface) const {
	VkBool32 ret = VK_FALSE;
	if (_vkDevice->getInstance()->vkGetPhysicalDeviceSurfaceSupportKHR(_vkDevice->getPhysicalDevice(),
			_vkDevice->getQueueFamily(QueueOperations::Graphics)->index, surface, &ret) == VK_SUCCESS) {
		return ret;
	}
	return false;
}

void ViewImpl::setIMEKeyboardState(bool open) {

}

void ViewImpl::setScreenSize(float width, float height) {
	View::setScreenSize(width, height);
}

void ViewImpl::setClipboardString(StringView str) {

}

StringView ViewImpl::getClipboardString() const {
	return StringView();
}

void ViewImpl::recreateSwapChain() {
	_glLoop->recreateSwapChain(_swapchain);
}

void ViewImpl::pushEvent(AppEvent::Value val) const {
	if ((val & AppEvent::Terminate) != 0) {
		log::text("View", "Terminate");
	}
	View::pushEvent(val);
	if (_view) {
		_view->onEventPushed();
	}
}

bool ViewImpl::poll() {
	return _view->poll();
}

void ViewImpl::close() {
	end();
}

Rc<gl::Swapchain> ViewImpl::makeSwapchain(const Rc<gl::RenderQueue> &queue) const {
	return Rc<Swapchain>::create(this, *_vkDevice, _surface, queue);
}

}


namespace stappler::xenolith::platform::graphic {

Rc<gl::View> createView(const Rc<EventLoop> &event, const Rc<gl::Loop> &loop, StringView viewName, URect rect) {
	return Rc<vk::ViewImpl>::create(event, loop, viewName, rect);
}

Rc<gl::View> createView(const Rc<EventLoop> &event, const Rc<gl::Loop> &loop, StringView viewName) {
	return Rc<vk::ViewImpl>::create(event, loop, viewName, URect());
}

gl::ImageFormat getCommonFormat() {
	return gl::ImageFormat::B8G8R8A8_UNORM;
}

}

#endif
