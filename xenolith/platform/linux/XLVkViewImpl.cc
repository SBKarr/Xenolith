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

#include "XLPlatformLinux.h"

#include <sys/eventfd.h>
#include <poll.h>

namespace stappler::xenolith::platform {

static Rc<LinuxViewInterface> ViewImpl_makeXcbView(XcbLibrary *lib, ViewImpl *view, StringView name, URect rect) {
	return Rc<XcbView>::alloc(lib, view, name, rect);
}

/*static Rc<LinuxViewInterface> ViewImpl_makeWaylandView(const Instance *instance, Device *device, ViewImpl *view,
		StringView name, URect rect) {
	return nullptr;
}*/

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() {
	_view = nullptr;
}

bool ViewImpl::init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	_rect = info.rect;
	_name = info.name;

	if (!View::init(static_cast<vk::Loop &>(loop), static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	return true;
}

void ViewImpl::threadInit() {

	// try wayland
	/*auto d = getenv("WAYLAND_DISPLAY");
	if (d) {
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
	if (auto xcb = XcbLibrary::getInstance()) {
		_view = ViewImpl_makeXcbView(xcb, this, _name, _rect);
		if (!_view) {
			log::text("VkView", "Fail to initialize xcb window");
			return;
		}

		_surface = Rc<vk::Surface>::create(_instance, _view->createWindowSurface(_instance), _view);
		_frameInterval = _view->getScreenFrameInterval();
	}

	View::threadInit();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
	_view = nullptr;
}

bool ViewImpl::worker() {
	_eventFd = eventfd(0, EFD_NONBLOCK);
	auto socket = _view->getSocketFd();

	int timeoutMin = 1;

	struct pollfd fds[2];

	fds[0].fd = _eventFd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	fds[1].fd = socket;
	fds[1].events = POLLIN;
	fds[1].revents = 0;

	update();

	while (_shouldQuit.test_and_set()) {
		bool shouldUpdate = false;
		int ret = ::poll( fds, 2, timeoutMin);
		if (ret > 0) {
			if (fds[0].revents != 0) {
				uint64_t value = 0;
				auto sz = ::read(_eventFd, &value, sizeof(uint64_t));
				if (sz == 8 && value) {
					shouldUpdate = true;
				}
				fds[0].revents = 0;
			}
			if (fds[1].revents != 0) {
				fds[1].revents = 0;
				if (!_view->poll()) {
					break;
				}
			}
		} else if (ret == 0) {
			shouldUpdate = true;
		} else if (errno != EINTR) {
			// ignore EINTR to allow debugging
			break;
		}

		if (shouldUpdate) {
			update();
		}
	}

	::close(_eventFd);
	return false;
}

void ViewImpl::wakeup() {
	if (_eventFd >= 0) {
		uint64_t value = 1;
		::write(_eventFd, (const void *)&value, sizeof(uint64_t));
	}
}

bool ViewImpl::pollInput() {
	if (!_view->poll()) {
		close();
		return false;
	}
	return true;
}

}

namespace stappler::xenolith::platform::graphic {

gl::ImageFormat getCommonFormat() {
	return gl::ImageFormat::B8G8R8A8_UNORM;
}

Rc<gl::View> createView(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	return Rc<platform::ViewImpl>::create(loop, dev, move(info));
}

}

#endif
