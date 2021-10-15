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

#ifndef LINUX_XCB
#define LINUX_XCB 1
#endif

#if LINUX_XCB
#include <xcb/xcb.h>
#endif

#include "XLGlSwapchain.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

class LinuxViewInterface : public Ref {
public:
	virtual bool isAvailableOnDevice(const Device &) const = 0;
	virtual VkSurfaceKHR createWindowSurface() = 0;

	virtual void onEventPushed() = 0;

	virtual int getEventFd() const = 0;
	virtual int getSocketFd() const = 0;

	virtual bool poll() = 0;
};

class ViewImpl : public gl::View {
public:
	ViewImpl();
    virtual ~ViewImpl();

    bool init(const Rc<EventLoop> &, const Rc<gl::Loop> &loop, StringView viewName, URect rect);

    virtual bool begin(const Rc<Director> &, Function<void()> &&) override;
	virtual void end() override;

	virtual bool isAvailableOnDevice(VkSurfaceKHR) const;
	virtual void setIMEKeyboardState(bool open) override;

	virtual void pushEvent(AppEvent::Value) const override;
	virtual bool poll() override;
	virtual void close() override;

	virtual void setScreenSize(float width, float height) override;

	virtual void setClipboardString(StringView) override;
	virtual StringView getClipboardString() const override;

	vk::Device *getVkDevice() const { return _vkDevice; }

	LinuxViewInterface *getView() const { return _view; }

	void recreateSwapChain();

protected:
	virtual Rc<gl::Swapchain> makeSwapchain(const Rc<gl::RenderQueue> &) const override;

	const vk::Instance *_vkInstance = nullptr;
	vk::Device *_vkDevice = nullptr;
	Rc<LinuxViewInterface> _view;

	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	uint32_t _frameWidth = 0;
	uint32_t _frameHeight = 0;
	uint64_t _frameTimeMicroseconds = 1000'000 / 60;
};


#if LINUX_XCB

class XcbConnectionCache {
public:
	struct ConnectionData {
		int screen_nbr = -1;
		xcb_connection_t *connection = nullptr;
		const xcb_setup_t *setup = nullptr;
		xcb_screen_t *screen = nullptr;
	};

	static ConnectionData getActive();
	static ConnectionData acquire();

	XcbConnectionCache();

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection();

protected:
	void openConnection(ConnectionData &data);

	ConnectionData _pending;
	ConnectionData _current;
};

static XcbConnectionCache *s_connectionCache = nullptr;

XcbConnectionCache::ConnectionData XcbConnectionCache::getActive() {
	if (!s_connectionCache) {
		s_connectionCache = new XcbConnectionCache;
	}
	return s_connectionCache->getActiveConnection();
}

XcbConnectionCache::ConnectionData XcbConnectionCache::acquire() {
	if (!s_connectionCache) {
		s_connectionCache = new XcbConnectionCache;
	}
	return s_connectionCache->acquireConnection();
}

XcbConnectionCache::XcbConnectionCache() {
	openConnection(_pending);
}

XcbConnectionCache::ConnectionData XcbConnectionCache::acquireConnection() {
	if (_pending.connection) {
		_current = _pending;
		_pending = ConnectionData({-1, nullptr, nullptr, nullptr});
		return _current;
	} else {
		openConnection(_current);
		return _current;
	}
}

XcbConnectionCache::ConnectionData XcbConnectionCache::getActiveConnection() {
	if (_pending.connection) {
		return _pending;
	} else {
		return _current;
	}
}

void XcbConnectionCache::openConnection(ConnectionData &data) {
	data.setup = nullptr;
	data.screen = nullptr;
	data.connection = xcb_connect(NULL, &data.screen_nbr); // always not null
	int screen_nbr = data.screen_nbr;
	data.setup = xcb_get_setup(data.connection);
	auto iter = xcb_setup_roots_iterator(data.setup);
	for (; iter.rem; --screen_nbr, xcb_screen_next(&iter)) {
		if (screen_nbr == 0) {
			data.screen = iter.data;
			break;
		}
	}
}

#endif

}

#endif
