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

#include "XLPlatformLinuxWayland.h"
#include "XLPlatformLinuxXcb.h"

#include <sys/eventfd.h>
#include <poll.h>

namespace stappler::xenolith::platform {

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
	auto presentMask = _device->getPresentatonMask();

	if (auto wayland = WaylandLibrary::getInstance()) {
		if ((platform::SurfaceType(presentMask) & platform::SurfaceType::Wayland) != platform::SurfaceType::None) {
			auto waylandDisplay = getenv("WAYLAND_DISPLAY");
			auto sessionType = getenv("XDG_SESSION_TYPE");

			if (waylandDisplay || strcasecmp("wayland", sessionType) == 0) {
				auto view = Rc<WaylandView>::alloc(wayland, this, _name, _rect);
				if (!view) {
					log::text("VkView", "Fail to initialize xcb window");
					return;
				}

				_view = view;
				_surface = Rc<vk::Surface>::create(_instance, _view->createWindowSurface(_instance), _view);
				_frameInterval = _view->getScreenFrameInterval();
			}
		}
	}

	if (!_view) {
		// try X11
		if (auto xcb = XcbLibrary::getInstance()) {
			if ((platform::SurfaceType(presentMask) & platform::SurfaceType::XCB) != platform::SurfaceType::None) {
				auto view = Rc<XcbView>::alloc(xcb, this, _name, _rect);
				if (!view) {
					log::text("VkView", "Fail to initialize xcb window");
					return;
				}

				_view = view;
				_surface = Rc<vk::Surface>::create(_instance, _view->createWindowSurface(_instance), _view);
				_frameInterval = _view->getScreenFrameInterval();
			}
		}
	}

	if (!_view) {
		log::text("View", "No available surface type");
	}

	View::threadInit();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
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
				if (!_view->poll(false)) {
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

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) {
	performOnThread([this] {
		_inputEnabled = true;
	}, this);
}

void ViewImpl::cancelTextInput() {
	performOnThread([this] {
		_inputEnabled = false;
	}, this);
}

void ViewImpl::presentWithQueue(vk::DeviceQueue &queue, Rc<ImageStorage> &&image) {
	auto &e = _swapchain->getImageInfo().extent;
	_view->commit(e.width, e.height);
	vk::View::presentWithQueue(queue, move(image));
}

bool ViewImpl::pollInput(bool frameReady) {
	if (!_view->poll(frameReady)) {
		close();
		return false;
	}
	return true;
}

gl::SurfaceInfo ViewImpl::getSurfaceOptions() const {
	gl::SurfaceInfo ret = vk::View::getSurfaceOptions();
	_view->onSurfaceInfo(ret);
	return ret;
}

void ViewImpl::mapWindow() {
	if (_view) {
		_view->mapWindow();
	}
}

void ViewImpl::finalize() {
	_view = nullptr;
	View::finalize();
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
