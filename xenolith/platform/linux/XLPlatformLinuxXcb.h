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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_

#include "XLPlatformLinux.h"

#if LINUX

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>

#define explicit _explicit;
#include <xcb/xkb.h>
#undef explicit

namespace stappler::xenolith::platform {

class XcbLibrary : public Ref {
public:
	static constexpr int RANDR_MAJOR_VERSION = XCB_RANDR_MAJOR_VERSION;
	static constexpr int RANDR_MINOR_VERSION = XCB_RANDR_MINOR_VERSION;

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

	bool hasRandr() const { return _randr; }
	bool hasKeysyms() const { return _keysyms; }
	bool hasXkb() const { return _xkb; }

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

	void * (* xcb_wait_for_reply) (xcb_connection_t *c, unsigned int request, xcb_generic_error_t **e) = nullptr;

	xcb_get_modifier_mapping_cookie_t (* xcb_get_modifier_mapping_unchecked) (xcb_connection_t *c) = nullptr;
	xcb_get_modifier_mapping_reply_t * (* xcb_get_modifier_mapping_reply) (xcb_connection_t *c,
			xcb_get_modifier_mapping_cookie_t cookie, xcb_generic_error_t **e) = nullptr;
	xcb_keycode_t * (* xcb_get_modifier_mapping_keycodes) (const xcb_get_modifier_mapping_reply_t *) = nullptr;

	xcb_get_keyboard_mapping_cookie_t (*xcb_get_keyboard_mapping)(xcb_connection_t *c,
			xcb_keycode_t first_keycode, uint8_t count) = nullptr;
	xcb_get_keyboard_mapping_reply_t * (* xcb_get_keyboard_mapping_reply) (xcb_connection_t *c,
			xcb_get_keyboard_mapping_cookie_t cookie /**< */, xcb_generic_error_t **e) = nullptr;

	xcb_randr_query_version_cookie_t (* xcb_randr_query_version) (xcb_connection_t *c,
			uint32_t major_version, uint32_t minor_version) = nullptr;
	xcb_randr_query_version_reply_t * (* xcb_randr_query_version_reply) (xcb_connection_t *c,
			xcb_randr_query_version_cookie_t cookie, xcb_generic_error_t **e) = nullptr;
	xcb_randr_get_screen_info_cookie_t (* xcb_randr_get_screen_info_unchecked) (xcb_connection_t *c,
			xcb_window_t window) = nullptr;
	xcb_randr_get_screen_info_reply_t * (* xcb_randr_get_screen_info_reply) (xcb_connection_t *c,
			xcb_randr_get_screen_info_cookie_t cookie, xcb_generic_error_t **e) = nullptr;

	xcb_randr_screen_size_t * (* xcb_randr_get_screen_info_sizes) (const xcb_randr_get_screen_info_reply_t *) = nullptr;
	int (* xcb_randr_get_screen_info_sizes_length) (const xcb_randr_get_screen_info_reply_t *) = nullptr;
	xcb_randr_screen_size_iterator_t (* xcb_randr_get_screen_info_sizes_iterator) (const xcb_randr_get_screen_info_reply_t *) = nullptr;

	int (* xcb_randr_get_screen_info_rates_length) (const xcb_randr_get_screen_info_reply_t *) = nullptr;
	xcb_randr_refresh_rates_iterator_t (* xcb_randr_get_screen_info_rates_iterator) (const xcb_randr_get_screen_info_reply_t *) = nullptr;
	void (* xcb_randr_refresh_rates_next) (xcb_randr_refresh_rates_iterator_t *) = nullptr;
	uint16_t * (* xcb_randr_refresh_rates_rates) (const xcb_randr_refresh_rates_t *) = nullptr;
	int (* xcb_randr_refresh_rates_rates_length) (const xcb_randr_refresh_rates_t *) = nullptr;

	xcb_key_symbols_t * (* xcb_key_symbols_alloc) (xcb_connection_t *c) = nullptr;
	void (* xcb_key_symbols_free) (xcb_key_symbols_t *syms) = nullptr;

	xcb_keysym_t (* xcb_key_symbols_get_keysym) (xcb_key_symbols_t *syms, xcb_keycode_t keycode, int col) = nullptr;
	xcb_keycode_t * (* xcb_key_symbols_get_keycode) (xcb_key_symbols_t *syms, xcb_keysym_t keysym) = nullptr;
	xcb_keysym_t (* xcb_key_press_lookup_keysym) (xcb_key_symbols_t *syms, xcb_key_press_event_t *event, int col) = nullptr;
	xcb_keysym_t (* xcb_key_release_lookup_keysym) (xcb_key_symbols_t *syms, xcb_key_release_event_t *event, int col) = nullptr;
	int (* xcb_refresh_keyboard_mapping) (xcb_key_symbols_t *syms, xcb_mapping_notify_event_t *event) = nullptr;

	int (* xcb_is_keypad_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_private_keypad_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_cursor_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_pf_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_function_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_misc_function_key) (xcb_keysym_t keysym) = nullptr;
	int (* xcb_is_modifier_key) (xcb_keysym_t keysym) = nullptr;

	xcb_void_cookie_t (* xcb_xkb_select_events) (xcb_connection_t *c, xcb_xkb_device_spec_t deviceSpec,
			uint16_t affectWhich, uint16_t clear, uint16_t selectAll, uint16_t affectMap, uint16_t map, const void *details) = nullptr;

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection() const;

protected:
	void openAux();
	void openConnection(ConnectionData &data);

	void *_handle = nullptr;
	void *_randr = nullptr;
	void *_keysyms = nullptr;
	void *_xkb = nullptr;

	ConnectionData _pending;
	ConnectionData _current;
};

struct XcbAtomRequest {
	StringView name;
	bool onlyIfExists;
};

static XcbAtomRequest s_atomRequests[] = {
	{ "WM_PROTOCOLS", true },
	{ "WM_DELETE_WINDOW", false },
	{ "WM_NAME", false },
	{ "WM_ICON_NAME", false },
};

class XcbView : public LinuxViewInterface {
public:
	struct ScreenInfo {
	    uint16_t width;
	    uint16_t height;
	    uint16_t mwidth;
	    uint16_t mheight;
	    Vector<uint16_t> rates;
	};

	static void ReportError(int error);

	XcbView(XcbLibrary *, ViewImpl *, StringView, URect);
	virtual ~XcbView();

	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const override;

	bool valid() const;

	virtual bool poll() override;

	virtual int getSocketFd() const override { return _socket; }

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

protected:
	Vector<ScreenInfo> getScreenInfo() const;

	void initXkb();

	void updateXkbMapping();
	void updateKeysymMapping();
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods = true);

	void updateXkbKey(xcb_keycode_t code);

	// get code from keysym mapping table
	InputKeyCode getKeyCode(xcb_keycode_t code) const;

	// map keysym to InputKeyCode
	InputKeyCode getKeysymCode(xcb_keysym_t sym) const;

	Rc<XcbLibrary> _xcb;
	Rc<XkbLibrary> _xkb;
	ViewImpl *_view = nullptr;
	xcb_connection_t *_connection = nullptr;
	xcb_screen_t *_defaultScreen = nullptr;
	xcb_key_symbols_t *_keysyms = nullptr;
	uint32_t _window = 0;

	xcb_atom_t _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	uint16_t _width = 0;
	uint16_t _height = 0;
	uint16_t _rate = 60;

	int _socket = -1;

	uint16_t _numlock = 0;
	uint16_t _shiftlock = 0;
	uint16_t _capslock = 0;
	uint16_t _modeswitch = 0;

	bool _xcbSetup = false;
	int32_t _xkbDeviceId = 0;
	uint8_t _xkbFirstEvent = 0;
	uint8_t _xkbFirstError = 0;
	xkb_keymap *_xkbKeymap = nullptr;
	xkb_state *_xkbState = nullptr;
	InputKeyCode _keycodes[256] = { InputKeyCode::Unknown };
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXCB_H_ */
