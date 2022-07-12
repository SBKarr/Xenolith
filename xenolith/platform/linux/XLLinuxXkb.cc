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

#include "XLPlatformLinux.h"

#include <dlfcn.h>

namespace stappler::xenolith::platform {

XkbLibrary *XkbLibrary::getInstance() {
	static Mutex s_mutex;
	static Rc<XkbLibrary> s_lib;

	std::unique_lock<Mutex> lock(s_mutex);
	if (!s_lib) {
		s_lib = Rc<XkbLibrary>::create();
	}

	return s_lib;
}

XkbLibrary::~XkbLibrary() {
	close();
}

bool XkbLibrary::init() {
	if (auto d = dlopen("libxkbcommon.so", RTLD_LAZY)) {
		if (open(d)) {
			_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			return true;
		}
		dlclose(d);
	}
	return false;
}

void XkbLibrary::close() {
	if (_context) {
		xkb_context_unref(_context);
		_context = nullptr;
	}
	if (_handle) {
		dlclose(_handle);
		_handle = nullptr;
	}
}

bool XkbLibrary::open(void *handle) {
	xkb_context_new = reinterpret_cast<decltype(xkb_context_new)>(dlsym(handle, "xkb_context_new"));
	xkb_context_ref = reinterpret_cast<decltype(xkb_context_ref)>(dlsym(handle, "xkb_context_ref"));
	xkb_context_unref = reinterpret_cast<decltype(xkb_context_unref)>(dlsym(handle, "xkb_context_unref"));
	xkb_keymap_unref = reinterpret_cast<decltype(xkb_keymap_unref)>(dlsym(handle, "xkb_keymap_unref"));
	xkb_state_unref = reinterpret_cast<decltype(xkb_state_unref)>(dlsym(handle, "xkb_state_unref"));
	xkb_keymap_new_from_string = reinterpret_cast<decltype(xkb_keymap_new_from_string)>(dlsym(handle, "xkb_keymap_new_from_string"));
	xkb_state_new = reinterpret_cast<decltype(xkb_state_new)>(dlsym(handle, "xkb_state_new"));
	xkb_state_update_mask = reinterpret_cast<decltype(xkb_state_update_mask)>(dlsym(handle, "xkb_state_update_mask"));
	xkb_state_key_get_utf8 = reinterpret_cast<decltype(xkb_state_key_get_utf8)>(dlsym(handle, "xkb_state_key_get_utf8"));
	xkb_state_key_get_utf32 = reinterpret_cast<decltype(xkb_state_key_get_utf32)>(dlsym(handle, "xkb_state_key_get_utf32"));
	xkb_state_key_get_one_sym = reinterpret_cast<decltype(xkb_state_key_get_one_sym)>(dlsym(handle, "xkb_state_key_get_one_sym"));
	xkb_state_mod_index_is_active = reinterpret_cast<decltype(xkb_state_mod_index_is_active)>(dlsym(handle, "xkb_state_mod_index_is_active"));
	xkb_state_key_get_syms = reinterpret_cast<decltype(xkb_state_key_get_syms)>(dlsym(handle, "xkb_state_key_get_syms"));
	xkb_state_get_keymap = reinterpret_cast<decltype(xkb_state_get_keymap)>(dlsym(handle, "xkb_state_get_keymap"));
	xkb_keymap_key_for_each = reinterpret_cast<decltype(xkb_keymap_key_for_each)>(dlsym(handle, "xkb_keymap_key_for_each"));
	xkb_keymap_key_get_name = reinterpret_cast<decltype(xkb_keymap_key_get_name)>(dlsym(handle, "xkb_keymap_key_get_name"));
	xkb_keymap_mod_get_index = reinterpret_cast<decltype(xkb_keymap_mod_get_index)>(dlsym(handle, "xkb_keymap_mod_get_index"));
	xkb_keymap_key_repeats = reinterpret_cast<decltype(xkb_keymap_key_repeats)>(dlsym(handle, "xkb_keymap_key_repeats"));
	xkb_keysym_to_utf32 = reinterpret_cast<decltype(xkb_keysym_to_utf32)>(dlsym(handle, "xkb_keysym_to_utf32"));
	xkb_compose_table_new_from_locale = reinterpret_cast<decltype(xkb_compose_table_new_from_locale)>(dlsym(handle, "xkb_compose_table_new_from_locale"));
	xkb_compose_table_unref = reinterpret_cast<decltype(xkb_compose_table_unref)>(dlsym(handle, "xkb_compose_table_unref"));
	xkb_compose_state_new = reinterpret_cast<decltype(xkb_compose_state_new)>(dlsym(handle, "xkb_compose_state_new"));
	xkb_compose_state_feed = reinterpret_cast<decltype(xkb_compose_state_feed)>(dlsym(handle, "xkb_compose_state_feed"));
	xkb_compose_state_get_status = reinterpret_cast<decltype(xkb_compose_state_get_status)>(dlsym(handle, "xkb_compose_state_get_status"));
	xkb_compose_state_get_one_sym = reinterpret_cast<decltype(xkb_compose_state_get_one_sym)>(dlsym(handle, "xkb_compose_state_get_one_sym"));
	xkb_compose_state_unref = reinterpret_cast<decltype(xkb_compose_state_unref)>(dlsym(handle, "xkb_compose_state_unref"));

	if (this->xkb_context_new
			&& this->xkb_context_ref
			&& this->xkb_context_unref
			&& this->xkb_keymap_unref
			&& this->xkb_state_unref
			&& this->xkb_keymap_new_from_string
			&& this->xkb_state_new
			&& this->xkb_state_update_mask
			&& this->xkb_state_key_get_utf8
			&& this->xkb_state_key_get_utf32
			&& this->xkb_state_key_get_one_sym
			&& this->xkb_state_mod_index_is_active
			&& this->xkb_state_key_get_syms
			&& this->xkb_state_get_keymap
			&& this->xkb_keymap_key_for_each
			&& this->xkb_keymap_key_get_name
			&& this->xkb_keymap_mod_get_index
			&& this->xkb_keymap_key_repeats
			&& this->xkb_keysym_to_utf32
			&& this->xkb_compose_table_new_from_locale
			&& this->xkb_compose_table_unref
			&& this->xkb_compose_state_new
			&& this->xkb_compose_state_feed
			&& this->xkb_compose_state_get_status
			&& this->xkb_compose_state_get_one_sym
			&& this->xkb_compose_state_unref) {
		_handle = handle;
		openAux();
		return true;
	}
	return false;
}

void XkbLibrary::openAux() {
	if (auto d = dlopen("libxkbcommon-x11.so", RTLD_LAZY)) {
		this->xkb_x11_setup_xkb_extension =
				reinterpret_cast<decltype(this->xkb_x11_setup_xkb_extension)>(dlsym(d, "xkb_x11_setup_xkb_extension"));
		this->xkb_x11_get_core_keyboard_device_id =
				reinterpret_cast<decltype(this->xkb_x11_get_core_keyboard_device_id)>(dlsym(d, "xkb_x11_get_core_keyboard_device_id"));
		this->xkb_x11_keymap_new_from_device =
				reinterpret_cast<decltype(this->xkb_x11_keymap_new_from_device)>(dlsym(d, "xkb_x11_keymap_new_from_device"));
		this->xkb_x11_state_new_from_device =
				reinterpret_cast<decltype(this->xkb_x11_state_new_from_device)>(dlsym(d, "xkb_x11_state_new_from_device"));

		if (this->xkb_x11_setup_xkb_extension
				&& xkb_x11_get_core_keyboard_device_id
				&& xkb_x11_keymap_new_from_device
				&& xkb_x11_state_new_from_device) {
			_x11 = d;
		} else {
			this->xkb_x11_setup_xkb_extension = nullptr;
			this->xkb_x11_get_core_keyboard_device_id = nullptr;
			this->xkb_x11_keymap_new_from_device = nullptr;
			this->xkb_x11_state_new_from_device = nullptr;

			dlclose(d);
		}
	}
}

}

#endif
