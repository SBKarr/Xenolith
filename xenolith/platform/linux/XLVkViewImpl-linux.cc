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

static Rc<LinuxViewInterface> ViewImpl_makeXcbView(const Instance *instance, Device *device, ViewImpl *view,
		StringView name, URect rect) {
	auto v = Rc<XcbView>::alloc(instance, view, name, rect);
	if (!v->isAvailableOnDevice(*device)) {
		return nullptr;
	}
	return v;
}

static Rc<LinuxViewInterface> ViewImpl_makeWaylandView(const Instance *instance, Device *device, ViewImpl *view,
		StringView name, URect rect) {
	return nullptr;
}

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

	// try wayland
	auto d = getenv("WAYLAND_DISPLAY");
	/*if (d) {
		_view = ViewImpl_makeWaylandView(_vkInstance, _vkDevice, this, viewName, rect);
		if (_view) {
			return gl::View::init(ev, loop);
		}
		log::text("VkView", "Fail to initialize Wayland window");
		return false;
	}

	d = getenv("XDG_SESSION_TYPE");
	if (strcasecmp("wayland", d)) {
		_view = ViewImpl_makeWaylandView(_vkInstance, _vkDevice, this, viewName, rect);
		if (_view) {
			return gl::View::init(ev, loop);
		}
		log::text("VkView", "Fail to initialize Wayland window");
		return false;
	}*/

	// try X11
	_view = ViewImpl_makeXcbView(_vkInstance, _vkDevice, this, viewName, rect);
	if (!_view) {
		log::text("VkView", "Fail to initialize xcb window");
		return false;
	}
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

	_director = director;
	_swapchain = Rc<Swapchain>::create(this, *_vkDevice, _surface).get();

	if (View::begin(director, move(cb))) {
		return true;
	}

	return false;
}

void ViewImpl::end() {
	View::end();
	_swapchain->invalidate(*_vkDevice);
	_swapchain = nullptr;
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

void ViewImpl::setScreenExtent(Extent2 e) {
	View::setScreenExtent(e);
}

void ViewImpl::setClipboardString(StringView str) {

}

StringView ViewImpl::getClipboardString() const {
	return StringView();
}

void ViewImpl::recreateSwapChain() {
	_swapchain->deprecate();
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
