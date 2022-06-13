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

#include <dlfcn.h>

namespace stappler::xenolith::platform {

static XcbLibrary *s_XcbLibrary = nullptr;

XcbLibrary *XcbLibrary::getInstance() {
	return s_XcbLibrary;
}

XcbLibrary::~XcbLibrary() {
	close();
}

bool XcbLibrary::init() {
	if (auto d = dlopen("libxcb.so", RTLD_LAZY)) {
		if (open(d)) {
			s_XcbLibrary = this;
			openConnection(_pending);
			return true;
		}
		dlclose(d);
	}
	return false;
}

bool XcbLibrary::open(void *handle) {
	this->xcb_connect = reinterpret_cast<decltype(this->xcb_connect)>(dlsym(handle, "xcb_connect"));
	this->xcb_get_setup = reinterpret_cast<decltype(this->xcb_get_setup)>(dlsym(handle, "xcb_get_setup"));
	this->xcb_setup_roots_iterator = reinterpret_cast<decltype(this->xcb_setup_roots_iterator)>(dlsym(handle, "xcb_setup_roots_iterator"));
	this->xcb_screen_next = reinterpret_cast<decltype(this->xcb_screen_next)>(dlsym(handle, "xcb_screen_next"));
	this->xcb_connection_has_error = reinterpret_cast<decltype(this->xcb_connection_has_error)>(dlsym(handle, "xcb_connection_has_error"));
	this->xcb_get_file_descriptor = reinterpret_cast<decltype(this->xcb_get_file_descriptor)>(dlsym(handle, "xcb_get_file_descriptor"));
	this->xcb_generate_id = reinterpret_cast<decltype(this->xcb_generate_id)>(dlsym(handle, "xcb_generate_id"));
	this->xcb_flush = reinterpret_cast<decltype(this->xcb_flush)>(dlsym(handle, "xcb_flush"));
	this->xcb_disconnect = reinterpret_cast<decltype(this->xcb_disconnect)>(dlsym(handle, "xcb_disconnect"));
	this->xcb_poll_for_event = reinterpret_cast<decltype(this->xcb_poll_for_event)>(dlsym(handle, "xcb_poll_for_event"));
	this->xcb_map_window = reinterpret_cast<decltype(this->xcb_map_window)>(dlsym(handle, "xcb_map_window"));
	this->xcb_create_window = reinterpret_cast<decltype(this->xcb_create_window)>(dlsym(handle, "xcb_create_window"));
	this->xcb_change_property = reinterpret_cast<decltype(this->xcb_change_property)>(dlsym(handle, "xcb_change_property"));
	this->xcb_intern_atom = reinterpret_cast<decltype(this->xcb_intern_atom)>(dlsym(handle, "xcb_intern_atom"));
	this->xcb_intern_atom_reply = reinterpret_cast<decltype(this->xcb_intern_atom_reply)>(dlsym(handle, "xcb_intern_atom_reply"));

	if (this->xcb_connect
			&& this->xcb_get_setup
			&& this->xcb_setup_roots_iterator
			&& this->xcb_screen_next
			&& this->xcb_connection_has_error
			&& this->xcb_get_file_descriptor
			&& this->xcb_generate_id
			&& this->xcb_flush
			&& this->xcb_disconnect
			&& this->xcb_poll_for_event
			&& this->xcb_map_window
			&& this->xcb_create_window
			&& this->xcb_change_property
			&& this->xcb_intern_atom
			&& this->xcb_intern_atom_reply) {
		return true;
	}
	return false;
}

void XcbLibrary::close() {
	if (_pending.connection) {
		xcb_disconnect(_pending.connection);
	}

	if (s_XcbLibrary == this) {
		s_XcbLibrary = nullptr;
	}
	if (_handle) {
		dlclose(_handle);
		_handle = nullptr;
	}
}

XcbLibrary::ConnectionData XcbLibrary::acquireConnection() {
	if (_pending.connection) {
		_current = _pending;
		_pending = ConnectionData({-1, nullptr, nullptr, nullptr});
		return _current;
	} else {
		openConnection(_current);
		return _current;
	}
}

XcbLibrary::ConnectionData XcbLibrary::getActiveConnection() {
	if (_pending.connection) {
		return _pending;
	} else {
		return _current;
	}
}

void XcbLibrary::openConnection(ConnectionData &data) {
	data.setup = nullptr;
	data.screen = nullptr;
	data.connection = this->xcb_connect(NULL, &data.screen_nbr); // always not null
	int screen_nbr = data.screen_nbr;
	data.setup = this->xcb_get_setup(data.connection);
	auto iter = this->xcb_setup_roots_iterator(data.setup);
	for (; iter.rem; --screen_nbr, this->xcb_screen_next(&iter)) {
		if (screen_nbr == 0) {
			data.screen = iter.data;
			break;
		}
	}
}

}

#endif
