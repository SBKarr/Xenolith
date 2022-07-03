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

#include "XLDefine.h"

#if LINUX

#include "XLPlatformLinuxWayland.h"

#include <dlfcn.h>

namespace stappler::xenolith::platform {

static WaylandLibrary *s_WaylandLibrary = nullptr;

WaylandLibrary *WaylandLibrary::getInstance() {
	return s_WaylandLibrary;
}

WaylandLibrary::~WaylandLibrary() {
	close();
}

WaylandLibrary::WaylandLibrary() {

}

bool WaylandLibrary::init() {
	if (auto d = dlopen("libwayland-client.so", RTLD_LAZY)) {
		if (open(d)) {
			s_WaylandLibrary = this;
			openConnection(_pending);
			return _pending.display != nullptr;
		}
		dlclose(d);
	}
	return false;
}

void WaylandLibrary::close() {
	if (_pending.display) {
		wl_display_disconnect(_pending.display);
		_pending.display = nullptr;
	}

	if (xdg) {
		delete xdg;
		xdg = nullptr;
	}

	if (s_WaylandLibrary == this) {
		s_WaylandLibrary = nullptr;
	}

	if (_handle) {
		dlclose(_handle);
		_handle = nullptr;
	}
}

WaylandLibrary::ConnectionData WaylandLibrary::acquireConnection() {
	if (_pending.display) {
		_current = _pending;
		_pending = ConnectionData({nullptr});
		return _current;
	} else {
		openConnection(_current);
		return _current;
	}
}

WaylandLibrary::ConnectionData WaylandLibrary::getActiveConnection() const {
	if (_pending.display) {
		return _pending;
	} else {
		return _current;
	}
}


bool WaylandLibrary::open(void *handle) {
	this->wl_registry_interface = reinterpret_cast<decltype(this->wl_registry_interface)>(dlsym(handle, "wl_registry_interface"));
	this->wl_compositor_interface = reinterpret_cast<decltype(this->wl_compositor_interface)>(dlsym(handle, "wl_compositor_interface"));
	this->wl_output_interface = reinterpret_cast<decltype(this->wl_output_interface)>(dlsym(handle, "wl_output_interface"));
	this->wl_seat_interface = reinterpret_cast<decltype(this->wl_seat_interface)>(dlsym(handle, "wl_seat_interface"));
	this->wl_surface_interface = reinterpret_cast<decltype(this->wl_surface_interface)>(dlsym(handle, "wl_surface_interface"));
	this->wl_region_interface = reinterpret_cast<decltype(this->wl_region_interface)>(dlsym(handle, "wl_region_interface"));
	this->wl_callback_interface = reinterpret_cast<decltype(this->wl_callback_interface)>(dlsym(handle, "wl_callback_interface"));

	this->wl_display_connect = reinterpret_cast<decltype(this->wl_display_connect)>(dlsym(handle, "wl_display_connect"));
	this->wl_display_get_fd = reinterpret_cast<decltype(this->wl_display_get_fd)>(dlsym(handle, "wl_display_get_fd"));
	this->wl_display_dispatch = reinterpret_cast<decltype(this->wl_display_dispatch)>(dlsym(handle, "wl_display_dispatch"));
	this->wl_display_dispatch_pending = reinterpret_cast<decltype(this->wl_display_dispatch_pending)>(dlsym(handle, "wl_display_dispatch_pending"));
	this->wl_display_prepare_read = reinterpret_cast<decltype(this->wl_display_prepare_read)>(dlsym(handle, "wl_display_prepare_read"));
	this->wl_display_flush = reinterpret_cast<decltype(this->wl_display_flush)>(dlsym(handle, "wl_display_flush"));
	this->wl_display_read_events = reinterpret_cast<decltype(this->wl_display_read_events)>(dlsym(handle, "wl_display_read_events"));
	this->wl_display_disconnect = reinterpret_cast<decltype(this->wl_display_disconnect)>(dlsym(handle, "wl_display_disconnect"));
	this->wl_proxy_marshal_flags = reinterpret_cast<decltype(this->wl_proxy_marshal_flags)>(dlsym(handle, "wl_proxy_marshal_flags"));
	this->wl_proxy_get_version = reinterpret_cast<decltype(this->wl_proxy_get_version)>(dlsym(handle, "wl_proxy_get_version"));
	this->wl_proxy_add_listener = reinterpret_cast<decltype(this->wl_proxy_add_listener)>(dlsym(handle, "wl_proxy_add_listener"));
	this->wl_proxy_destroy = reinterpret_cast<decltype(this->wl_proxy_destroy)>(dlsym(handle, "wl_proxy_destroy"));
	this->wl_display_roundtrip = reinterpret_cast<decltype(this->wl_display_roundtrip)>(dlsym(handle, "wl_display_roundtrip"));

	if (this->wl_registry_interface
			&& this->wl_compositor_interface
			&& this->wl_output_interface
			&& this->wl_seat_interface
			&& this->wl_surface_interface
			&& this->wl_region_interface
			&& this->wl_callback_interface
			&& this->wl_display_connect
			&& this->wl_display_get_fd
			&& this->wl_display_dispatch
			&& this->wl_display_disconnect
			&& this->wl_proxy_marshal_flags
			&& this->wl_proxy_get_version
			&& this->wl_proxy_add_listener
			&& this->wl_proxy_destroy
			&& this->wl_display_roundtrip
			&& this->wl_registry_interface) {
		xdg = new XdgInterface(wl_output_interface, wl_seat_interface, wl_surface_interface);

		xdg_wm_base_interface = &xdg->xdg_wm_base_interface;
		xdg_positioner_interface = &xdg->xdg_positioner_interface;
		xdg_surface_interface = &xdg->xdg_surface_interface;
		xdg_toplevel_interface = &xdg->xdg_toplevel_interface;
		xdg_popup_interface = &xdg->xdg_popup_interface;

		_handle = handle;
		return true;
	}
	return false;
}

void WaylandLibrary::openConnection(ConnectionData &data) {
	data.display = wl_display_connect(NULL);
}

}

#endif
