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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_

#include "XLPlatform.h"

#if LINUX

#include "XLVkView.h"

#include <xcb/xcb.h>

namespace stappler::xenolith::platform {

class XcbLibrary : public Ref {
public:
	struct ConnectionData {
		int screen_nbr = -1;
		xcb_connection_t *connection = nullptr;
		const xcb_setup_t *setup = nullptr;
		xcb_screen_t *screen = nullptr;
	};

	static XcbLibrary *getInstance();

	XcbLibrary() { }

	virtual ~XcbLibrary();

	bool init();

	bool open(void *handle);
	void close();

	xcb_connection_t * (* xcb_connect) (const char *displayname, int *screenp) = nullptr;
	const struct xcb_setup_t * (* xcb_get_setup) (xcb_connection_t *c) = nullptr;
	xcb_screen_iterator_t (* xcb_setup_roots_iterator) (const xcb_setup_t *R) = nullptr;
	void (* xcb_screen_next) (xcb_screen_iterator_t *i) = nullptr;

	int (* xcb_connection_has_error) (xcb_connection_t *c) = nullptr;
	int (* xcb_get_file_descriptor) (xcb_connection_t *c) = nullptr;
	uint32_t (* xcb_generate_id) (xcb_connection_t *c) = nullptr;
	int (*xcb_flush) (xcb_connection_t *c) = nullptr;
	void (* xcb_disconnect) (xcb_connection_t *c) = nullptr;
	xcb_generic_event_t * (* xcb_poll_for_event) (xcb_connection_t *c) = nullptr;

	xcb_void_cookie_t (* xcb_map_window) (xcb_connection_t *c, xcb_window_t window) = nullptr;

	xcb_void_cookie_t (* xcb_create_window) (xcb_connection_t *c, uint8_t depth, xcb_window_t wid,
		xcb_window_t parent, int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t border_width,
		uint16_t _class, xcb_visualid_t visual, uint32_t value_mask, const void *value_list) = nullptr;

	xcb_void_cookie_t (* xcb_change_property) (xcb_connection_t *c, uint8_t mode, xcb_window_t window,
			xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data) = nullptr;

	xcb_intern_atom_cookie_t (* xcb_intern_atom) (xcb_connection_t *c, uint8_t only_if_exists,
			uint16_t name_len,const char *name) = nullptr;

	xcb_intern_atom_reply_t * (* xcb_intern_atom_reply) (xcb_connection_t *c, xcb_intern_atom_cookie_t cookie,
			xcb_generic_error_t **e) = nullptr;

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection();

protected:
	void *_handle = nullptr;

	void openConnection(ConnectionData &data);

	ConnectionData _pending;
	ConnectionData _current;
};

class LinuxViewInterface : public Ref {
public:
	virtual ~LinuxViewInterface() { }
	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const = 0;

	virtual int getSocketFd() const = 0;

	virtual bool poll() = 0;
};

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void wakeup() override;

	LinuxViewInterface *getView() const { return _view; }

	vk::Device *getDevice() const { return _device; }

protected:
	virtual bool pollInput() override;

	Rc<LinuxViewInterface> _view;
	URect _rect;
	String _name;
	int _eventFd = -1;
};

struct XcbAtomRequest {
	StringView name;
	bool onlyIfExists;
};

static XcbAtomRequest s_atomRequests[] = {
	{ "WM_PROTOCOLS", true },
	{ "WM_DELETE_WINDOW", false },
};

class XcbView : public LinuxViewInterface {
public:
	static void ReportError(int error);

	XcbView(XcbLibrary *, ViewImpl *, StringView, URect);
	virtual ~XcbView();

	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const override;

	bool valid() const;

	/*virtual bool isAvailableOnDevice(const Device &) const override;
	virtual VkSurfaceKHR createWindowSurface() override;*/

	virtual bool poll() override;

	virtual int getSocketFd() const override { return _socket; }

protected:
	Rc<XcbLibrary> _xcb;
	ViewImpl *_view = nullptr;
	xcb_connection_t *_connection = nullptr;
	xcb_screen_t *_defaultScreen = nullptr;
	uint32_t _window = 0;

	xcb_atom_t _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	uint16_t _width = 0;
	uint16_t _height = 0;

	int _socket = -1;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_ */
