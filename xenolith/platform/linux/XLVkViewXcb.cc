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

#include "XLVkInstance.h"
#include "XLPlatformLinuxXcb.h"
#include "XLInputDispatcher.h"
#include <X11/keysym.h>

namespace stappler::xenolith::platform {

void XcbView::ReportError(int error) {
	switch (error) {
	case XCB_CONN_ERROR:
		stappler::log::text("XcbView", "XCB_CONN_ERROR: socket error, pipe error or other stream error");
		break;
	case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_EXT_NOTSUPPORTED: extension is not supported");
		break;
	case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_MEM_INSUFFICIENT: out of memory");
		break;
	case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_REQ_LEN_EXCEED: too large request");
		break;
	case XCB_CONN_CLOSED_PARSE_ERR:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_PARSE_ERR: error during parsing display string");
		break;
	case XCB_CONN_CLOSED_INVALID_SCREEN:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_INVALID_SCREEN: server does not have a screen matching the display");
		break;
	case XCB_CONN_CLOSED_FDPASSING_FAILED:
		stappler::log::text("XcbView", "XCB_CONN_CLOSED_FDPASSING_FAILED: fail to pass some FD");
		break;
	}
}

XcbView::XcbView(XcbLibrary *lib, ViewImpl *impl, StringView name, URect rect) {
	_xcb = lib;
	_xkb = XkbLibrary::getInstance();
	_view = impl;
	if constexpr (vk::s_printVkInfo) {
		auto d = getenv("DISPLAY");
		if (!d) {
			stappler::log::vtext("XcbView-Info", "DISPLAY is not defined");
		} else {
			// stappler::log::vtext("XcbView-Info", "Using DISPLAY: '", d, "'");
		}
	}

	auto connection = lib->acquireConnection();

	_connection = connection.connection;

	auto err = _xcb->xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return;
	}

	_defaultScreen = connection.screen;

	if (_xcb->hasRandr()) {
		auto versionCookie = _xcb->xcb_randr_query_version( _connection, XcbLibrary::RANDR_MAJOR_VERSION, XcbLibrary::RANDR_MINOR_VERSION);
		if (auto versionReply = _xcb->xcb_randr_query_version_reply( _connection, versionCookie, nullptr)) {
			if (versionReply->major_version == XcbLibrary::RANDR_MAJOR_VERSION) {
				auto screenResCurrentCookie = _xcb->xcb_randr_get_screen_resources_current_unchecked(_connection, _defaultScreen->root);
				auto screenResCookie = _xcb->xcb_randr_get_screen_resources_unchecked(_connection, _defaultScreen->root);

				auto replyCurrent = _xcb->xcb_randr_get_screen_resources_current_reply(_connection, screenResCurrentCookie, nullptr);
				auto reply = _xcb->xcb_randr_get_screen_resources_reply(_connection, screenResCookie, nullptr);

				auto modes = _xcb->xcb_randr_get_screen_resources_modes(reply);
				auto nmodes = _xcb->xcb_randr_get_screen_resources_modes_length(reply);
				while (nmodes > 0) {
					double vTotal = modes->vtotal;

					if (modes->mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
						/* doublescan doubles the number of lines */
						vTotal *= 2;
					}

					if (modes->mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
						/* interlace splits the frame into two fields */
						/* the field rate is what is typically reported by monitors */
						vTotal /= 2;
					}

					if (modes->htotal && vTotal) {
						_rate = (uint16_t)floor((double) modes->dot_clock / ((double) modes->htotal * (double) vTotal));
						break;
					}

					++ modes;
					-- nmodes;
				}

				::free(replyCurrent);
				::free(reply);
			}

			::free(versionReply);
		}
	}

	if (_xkb && _xkb->hasX11() && _xcb->hasXkb()) {
		initXkb();
	}

	_socket = _xcb->xcb_get_file_descriptor(_connection); // assume it's non-blocking

	uint32_t mask = /*XCB_CW_BACK_PIXEL | */ XCB_CW_EVENT_MASK;
	uint32_t values[1];
	//values[0] = _defaultScreen->white_pixel;
	values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |	XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_COLOR_MAP_CHANGE
			| XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	/* Ask for our window's Id */
	_window = _xcb->xcb_generate_id(_connection);

	_width = rect.width;
	_height = rect.height;

	_xcb->xcb_create_window(_connection,
		XCB_COPY_FROM_PARENT, // depth (same as root)
		_window, // window Id
		_defaultScreen->root, // parent window
		rect.x, rect.y, rect.width, rect.height,
		0, // border_width
		XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
		_defaultScreen->root_visual, // visual
		mask, values);

	xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom(_connection, it.onlyIfExists ? 1 : 0, it.name.size(), it.name.data());
		++i;
	}

	_xcb->xcb_flush(_connection);

	i = 0;
	for (auto &it : atomCookies) {
		auto reply = _xcb->xcb_intern_atom_reply(_connection, it, nullptr);
		if (reply) {
			_atoms[i] = reply->atom;
			free(reply);
		} else {
			_atoms[i] = 0;
		}
		++i;
	}

	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, name.size(), name.data());
	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8, name.size(), name.data());
	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, _atoms[0], 4, 32, 1, &_atoms[1] );

	updateKeysymMapping();

	_xcb->xcb_flush(_connection);

	impl->setScreenExtent(Extent2(_width, _height));
}

XcbView::~XcbView() {
	_defaultScreen = nullptr;
	if (_xkbKeymap) {
		_xkb->xkb_keymap_unref(_xkbKeymap);
		_xkbKeymap = nullptr;
	}
	if (_xkbState) {
		_xkb->xkb_state_unref(_xkbState);
		_xkbState = nullptr;
	}
	if (_keysyms) {
		_xcb->xcb_key_symbols_free(_keysyms);
		_keysyms = nullptr;
	}
	if (_connection) {
		_xcb->xcb_disconnect(_connection);
		_connection = nullptr;
	}
}

bool XcbView::valid() const {
	return _xcb->xcb_connection_has_error(_connection) == 0;
}

VkSurfaceKHR XcbView::createWindowSurface(vk::Instance *instance) const {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0, _connection, _window};
	if (instance->vkCreateXcbSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
		return nullptr;
	}
	return surface;
}

static InputModifier getModifiers(uint32_t mask) {
	InputModifier ret = InputModifier::None;
	InputModifier *mod, mods[] = { InputModifier::Shift, InputModifier::CapsLock, InputModifier::Ctrl, InputModifier::Alt,
			InputModifier::NumLock, InputModifier::Mod3, InputModifier::Mod4, InputModifier::Mod5,
			InputModifier::Button1, InputModifier::Button2, InputModifier::Button3, InputModifier::Button4, InputModifier::Button5 };
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			ret |= *mod;
		}
	}
	return ret;
}

static InputMouseButton getButton(xcb_button_t btn) {
	return InputMouseButton(btn);
}

bool XcbView::poll(bool frameReady) {
	bool ret = true;
	bool deprecateSwapchain = false;

	Vector<InputEventData> inputEvents;

	auto dispatchEvents = [&] {
		for (auto &it : inputEvents) {
			_view->handleInputEvent(it);
		}
		inputEvents.clear();
	};

	xcb_timestamp_t lastInputTime = 0;
	xcb_generic_event_t *e;
	while ((e = _xcb->xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case XCB_EXPOSE: {
			// xcb_expose_event_t *ev = (xcb_expose_event_t*) e;
			// printf("XCB_EXPOSE: Window %d exposed. Region to be redrawn at location (%d,%d), with dimension (%d,%d)\n",
			//		ev->window, ev->x, ev->y, ev->width, ev->height);
			break;
		}
		case XCB_BUTTON_PRESS:
			if (_window == ((xcb_button_press_event_t *)e)->event) {
				auto ev = (xcb_button_press_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getScreenExtent();
				auto mod = getModifiers(ev->state);
				auto btn = getButton(ev->detail);

				InputEventData event({
					ev->detail,
					InputEventName::Begin,
					btn,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				switch (btn) {
				case InputMouseButton::MouseScrollUp:
					event.event = InputEventName::Scroll;
					event.point.valueX = 0.0f; event.point.valueY = 10.0f;
					break;
				case InputMouseButton::MouseScrollDown:
					event.event = InputEventName::Scroll;
					event.point.valueX = 0.0f; event.point.valueY = -10.0f;
					break;
				case InputMouseButton::MouseScrollLeft:
					event.event = InputEventName::Scroll;
					event.point.valueX = 10.0f; event.point.valueY = 0.0f;
					break;
				case InputMouseButton::MouseScrollRight:
					event.event = InputEventName::Scroll;
					event.point.valueX = -10.0f; event.point.valueY = 0.0f;
					break;
				default:
					break;
				}

				inputEvents.emplace_back(event);
			}
			break;
		case XCB_BUTTON_RELEASE:
			if (_window == ((xcb_button_release_event_t *)e)->event) {
				auto ev = (xcb_button_release_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getScreenExtent();
				auto mod = getModifiers(ev->state);
				auto btn = getButton(ev->detail);

				InputEventData event({
					ev->detail,
					InputEventName::End,
					btn,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				switch (btn) {
				case InputMouseButton::MouseScrollUp:
				case InputMouseButton::MouseScrollDown:
				case InputMouseButton::MouseScrollLeft:
				case InputMouseButton::MouseScrollRight:
					break;
				default:
					inputEvents.emplace_back(event);
					break;
				}
			}
			break;
		case XCB_MOTION_NOTIFY:
			if (_window == ((xcb_motion_notify_event_t *)e)->event) {
				auto ev = (xcb_motion_notify_event_t *)e;
				if (lastInputTime != ev->time) {
					dispatchEvents();
					lastInputTime = ev->time;
				}

				auto ext = _view->getScreenExtent();
				auto mod = getModifiers(ev->state);

				InputEventData event({
					maxOf<uint32_t>(),
					InputEventName::MouseMove,
					InputMouseButton::None,
					mod,
					float(ev->event_x),
					float(ext.height - ev->event_y)
				});

				inputEvents.emplace_back(event);
			}
			break;
		case XCB_ENTER_NOTIFY: {
			xcb_enter_notify_event_t *ev = (xcb_enter_notify_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto ext = _view->getScreenExtent();
			inputEvents.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, true,
					Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
			// printf("Mouse entered window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_LEAVE_NOTIFY: {
			xcb_leave_notify_event_t *ev = (xcb_leave_notify_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto ext = _view->getScreenExtent();
			inputEvents.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, false,
					Vec2(float(ev->event_x), float(ext.height - ev->event_y))));
			// printf("Mouse left window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_FOCUS_IN: {
			xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
			inputEvents.emplace_back(InputEventData::BoolEvent(InputEventName::FocusGain, true));
			updateKeysymMapping();
			printf("XCB_FOCUS_IN: %d\n", ev->event);
			break;
		}
		case XCB_FOCUS_OUT: {
			xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
			inputEvents.emplace_back(InputEventData::BoolEvent(InputEventName::FocusGain, false));
			printf("XCB_FOCUS_OUT: %d\n", ev->event);
			break;
		}
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t *ev = (xcb_key_press_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto mod = getModifiers(ev->state);
			auto ext = _view->getScreenExtent();

			// in case of key autorepeat, ev->time will match
			// just replace event name from previous InputEventName::KeyReleased to InputEventName::KeyRepeated
			if (!inputEvents.empty() && inputEvents.back().event == InputEventName::KeyReleased) {
				auto &iev = inputEvents.back();
				if (iev.id == ev->time && iev.modifiers == mod && iev.x == float(ev->event_x)
						&& iev.y == float(ext.height - ev->event_y)
						&& iev.key.keysym == getKeysym(ev->detail, ev->state, false)) {
					iev.event = InputEventName::KeyRepeated;
					break;
				}
			}

			InputEventData event({
				ev->time,
				InputEventName::KeyPressed,
				InputMouseButton::None,
				mod,
				float(ev->event_x),
				float(ext.height - ev->event_y)
			});

			if (_xkb) {
				event.key.keycode = getKeyCode(ev->detail);
				event.key.keysym = getKeysym(ev->detail, ev->state, false);
				if (_view->isInputEnabled()) {
					event.key.keychar = _xkb->xkb_state_key_get_utf32(_xkbState, ev->detail);
				} else {
					event.key.keychar = 0;
				}
			} else {
				auto sym = getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
				event.key.keycode = getKeysymCode(sym);
				event.key.keysym = sym;
				if (_view->isInputEnabled()) {
					event.key.keychar = _glfwKeySym2Unicode(getKeysym(ev->detail, ev->state)); // use state-dependent keysym
				} else {
					event.key.keychar = 0;
				}
			}

			inputEvents.emplace_back(event);

			String str;
			string::utf8Encode(str, event.key.keychar);
			printf("Key pressed in window %d (%d) %04x '%s' %04x\n", ev->event, (int)ev->time, event.key.keysym,
				str.data(), event.key.keychar);
			break;
		}
		case XCB_KEY_RELEASE: {
			xcb_key_release_event_t *ev = (xcb_key_release_event_t*) e;
			if (lastInputTime != ev->time) {
				dispatchEvents();
				lastInputTime = ev->time;
			}

			auto mod = getModifiers(ev->state);
			auto ext = _view->getScreenExtent();

			InputEventData event({
				ev->time,
				InputEventName::KeyReleased,
				InputMouseButton::None,
				mod,
				float(ev->event_x),
				float(ext.height - ev->event_y)
			});

			if (_xkb) {
				event.key.keycode = getKeyCode(ev->detail);
				event.key.keysym = getKeysym(ev->detail, ev->state, false);
				if (_view->isInputEnabled()) {
					event.key.keychar = _xkb->xkb_state_key_get_utf32(_xkbState, ev->detail);
				} else {
					event.key.keychar = 0;
				}
			} else {
				auto sym = getKeysym(ev->detail, ev->state, false); // state-inpependent keysym
				event.key.keycode = getKeysymCode(sym);
				event.key.keysym = sym;
				if (_view->isInputEnabled()) {
					event.key.keychar = _glfwKeySym2Unicode(getKeysym(ev->detail, ev->state)); // use state-dependent keysym
				} else {
					event.key.keychar = 0;
				}
			}

			inputEvents.emplace_back(event);

			String str;
			string::utf8Encode(str, event.key.keychar);
			printf("Key released in window %d (%d) %04x '%s' %04x\n", ev->event, (int)ev->time, event.key.keysym,
					str.data(), event.key.keychar);
			break;
		}
		case XCB_VISIBILITY_NOTIFY: {
			xcb_visibility_notify_event_t *ev = (xcb_visibility_notify_event_t*) e;
			printf("XCB_VISIBILITY_NOTIFY: %d\n", ev->window);
			break;
		}
		case XCB_MAP_NOTIFY: {
			xcb_map_notify_event_t *ev = (xcb_map_notify_event_t*) e;
			printf("XCB_MAP_NOTIFY: %d\n", ev->event);
			break;
		}
		case XCB_REPARENT_NOTIFY: {
			xcb_reparent_notify_event_t *ev = (xcb_reparent_notify_event_t*) e;
			printf("XCB_REPARENT_NOTIFY: %d %d to %d\n", ev->event, ev->window, ev->parent);
			break;
		}
		case XCB_CONFIGURE_NOTIFY: {
			xcb_configure_notify_event_t *ev = (xcb_configure_notify_event_t*) e;
			// printf("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event, ev->window,
			//		ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width), uint32_t(ev->override_redirect));
			if (ev->width != _width || ev->height != _height) {
				_width = ev->width;
				_height = ev->height;
				deprecateSwapchain = true;
			}
			break;
		}
		case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t *ev = (xcb_client_message_event_t*) e;
			printf("XCB_CLIENT_MESSAGE: %d of type %d\n", ev->window, ev->type);
			if (ev->type == _atoms[0] && ev->data.data32[0] == _atoms[1]) {
				ret = false;
			}
			break;
		}
		case XCB_PROPERTY_NOTIFY: {
			// xcb_property_notify_event_t *ev = (xcb_property_notify_event_t*) e;
			// printf("XCB_PROPERTY_NOTIFY: %d of property %d\n", ev->window, ev->atom);
			break;
		}
		case XCB_MAPPING_NOTIFY: {
			xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t*) e;
			if (_keysyms) {
				_xcb->xcb_refresh_keyboard_mapping(_keysyms, ev);
			}
			printf("XCB_MAPPING_NOTIFY: %d %d %d\n", (int) ev->request, (int) ev->first_keycode, (int) ev->count);
			break;
		}
		default:
			if (et == _xkbFirstEvent) {
				switch (e->pad0) {
				case XCB_XKB_NEW_KEYBOARD_NOTIFY:
					initXkb();
					break;
				case XCB_XKB_MAP_NOTIFY:
					updateXkbMapping();
					break;
				case XCB_XKB_STATE_NOTIFY: {
					xcb_xkb_state_notify_event_t *ev = (xcb_xkb_state_notify_event_t*)e;
					_xkb->xkb_state_update_mask(_xkbState, ev->baseMods, ev->latchedMods, ev->lockedMods,
							ev->baseGroup, ev->latchedGroup, ev->lockedGroup);
					break;
				}
				}
			} else {
				/* Unknown event type, ignore it */
				printf("Unknown event: %d\n", et);
			}
			break;
		}

		/* Free the Generic Event */
		free(e);
	}

	dispatchEvents();

	if (deprecateSwapchain) {
		_view->deprecateSwapchain();
	}

	return ret;
}

uint64_t XcbView::getScreenFrameInterval() const {
	return 1'000'000 / _rate;
}

void XcbView::mapWindow() {
	_xcb->xcb_map_window(_connection, _window);
	_xcb->xcb_flush(_connection);
}

Vector<XcbView::ScreenInfo> XcbView::getScreenInfo() const {
	if (!_xcb->hasRandr()) {
		return Vector<XcbView::ScreenInfo>();
	}

	auto versionCookie = _xcb->xcb_randr_query_version( _connection, XcbLibrary::RANDR_MAJOR_VERSION, XcbLibrary::RANDR_MINOR_VERSION);
	if (auto versionReply = _xcb->xcb_randr_query_version_reply( _connection, versionCookie, nullptr)) {
		if (versionReply->major_version != XcbLibrary::RANDR_MAJOR_VERSION) {
			::free(versionReply);
			return Vector<XcbView::ScreenInfo>();
		}

		::free(versionReply);
	} else {
		return Vector<XcbView::ScreenInfo>();
	}

	xcb_randr_get_screen_info_cookie_t screenInfoCookie =
			_xcb->xcb_randr_get_screen_info_unchecked(_connection, _defaultScreen->root);
	auto reply = _xcb->xcb_randr_get_screen_info_reply(_connection, screenInfoCookie, nullptr);
	auto sizes = size_t(_xcb->xcb_randr_get_screen_info_sizes_length(reply));

	Vector<Vector<uint16_t>> ratesVec;

	auto ratesIt = _xcb->xcb_randr_get_screen_info_rates_iterator(reply);
	for ( ; ratesIt.rem; _xcb->xcb_randr_refresh_rates_next(&ratesIt)) {
		auto rates = _xcb->xcb_randr_refresh_rates_rates(ratesIt.data);
		auto len = _xcb->xcb_randr_refresh_rates_rates_length(ratesIt.data);

		Vector<uint16_t> tmp;

		while (len) {
			tmp.emplace_back(*rates);
			++ rates;
			-- len;
		}

		ratesVec.emplace_back(move(tmp));
	}

	Vector<ScreenInfo> ret;
	if (ratesVec.size() == sizes) {
		auto sizesData = _xcb->xcb_randr_get_screen_info_sizes(reply);
		for (size_t i = 0; i < sizes; ++ i) {
			ScreenInfo info { sizesData[i].width, sizesData[i].height, sizesData[i].mwidth, sizesData[i].mheight };

			if (ratesVec.size() == sizes) {
				info.rates = ratesVec[i];
			} else if (ratesVec.size() == 1) {
				info.rates = ratesVec[0];
			} else {
				info.rates = Vector<uint16_t>{ _rate };
			}

			ret.emplace_back(move(info));
		}
	}

	::free(reply);

	return ret;
}

static void look_for(uint16_t &mask, xcb_keycode_t *codes, xcb_keycode_t kc, int i) {
	if (mask == 0 && codes) {
		for (xcb_keycode_t *ktest = codes; *ktest; ktest++) {
			if (*ktest == kc) {
				mask = (uint16_t) (1 << i);
				break;
			}
		}
	}
}

void XcbView::initXkb() {
	uint16_t xkbMajorVersion = 0;
	uint16_t xkbMinorVersion = 0;

	if (!_xcbSetup) {
		if (_xkb->xkb_x11_setup_xkb_extension(_connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
				XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, &xkbMajorVersion, &xkbMinorVersion, &_xkbFirstEvent, &_xkbFirstError) != 1) {
			return;
		}
	}

	_xcbSetup = true;
	_xkbDeviceId = _xkb->xkb_x11_get_core_keyboard_device_id(_connection);

	enum {
		required_events = (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

		required_nkn_details = (XCB_XKB_NKN_DETAIL_KEYCODES),

		required_map_parts = (XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS | XCB_XKB_MAP_PART_MODIFIER_MAP
				| XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS | XCB_XKB_MAP_PART_KEY_ACTIONS
				| XCB_XKB_MAP_PART_VIRTUAL_MODS | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

		required_state_details = (XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH
				| XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE
				| XCB_XKB_STATE_PART_GROUP_LATCH | XCB_XKB_STATE_PART_GROUP_LOCK),
	};

	static const xcb_xkb_select_events_details_t details = {
			.affectNewKeyboard = required_nkn_details,
			.newKeyboardDetails = required_nkn_details,
			.affectState = required_state_details,
			.stateDetails = required_state_details
	};

	_xcb->xcb_xkb_select_events(_connection, _xkbDeviceId, required_events, 0,
			required_events, required_map_parts, required_map_parts, &details);

	updateXkbMapping();
}

void XcbView::updateXkbMapping() {
	if (_xkbState) {
		_xkb->xkb_state_unref(_xkbState);
		_xkbState = nullptr;
	}
	if (_xkbKeymap) {
		_xkb->xkb_keymap_unref(_xkbKeymap);
		_xkbKeymap = nullptr;
	}

	_xkbKeymap = _xkb->xkb_x11_keymap_new_from_device(_xkb->getContext(), _connection, _xkbDeviceId, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (_xkbKeymap == nullptr) {
		fprintf( stderr, "Failed to get Keymap for current keyboard device.\n");
		return;
	}

	_xkbState = _xkb->xkb_x11_state_new_from_device(_xkbKeymap, _connection, _xkbDeviceId);
	if (_xkbState == nullptr) {
		fprintf( stderr, "Failed to get state object for current keyboard device.\n");
		return;
	}

	memset(_keycodes, 0, sizeof(InputKeyCode) * 256);

	_xkb->xkb_keymap_key_for_each(_xkbKeymap, [] (struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
		((XcbView *)data)->updateXkbKey(key);
	}, this);
}

void XcbView::updateKeysymMapping() {
	if (_keysyms) {
		_xcb->xcb_key_symbols_free(_keysyms);
	}

	if (_xcb->hasKeysyms()) {
		_keysyms = _xcb->xcb_key_symbols_alloc( _connection );
	}

	if (!_keysyms) {
		return;
	}

	auto modifierCookie = _xcb->xcb_get_modifier_mapping_unchecked( _connection );

	xcb_get_keyboard_mapping_cookie_t mappingCookie;
	const xcb_setup_t* setup = _xcb->xcb_get_setup(_connection);

	if (!_xkb) {
		mappingCookie = _xcb->xcb_get_keyboard_mapping(_connection, setup->min_keycode, setup->max_keycode - setup->min_keycode + 1);
	}

	auto numlockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Num_Lock );
	auto shiftlockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Shift_Lock );
	auto capslockcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Caps_Lock );
	auto modeswitchcodes = _xcb->xcb_key_symbols_get_keycode( _keysyms, XK_Mode_switch );

	auto modmap_r = _xcb->xcb_get_modifier_mapping_reply( _connection, modifierCookie, nullptr );
	if (!modmap_r) {
		return;
	}

	xcb_keycode_t *modmap = _xcb->xcb_get_modifier_mapping_keycodes( modmap_r );

	_numlock = 0;
	_shiftlock = 0;
	_capslock = 0;
	_modeswitch = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < modmap_r->keycodes_per_modifier; j++) {
			xcb_keycode_t kc = modmap[i * modmap_r->keycodes_per_modifier + j];
			look_for(_numlock, numlockcodes, kc, i);
			look_for(_shiftlock, shiftlockcodes, kc, i);
			look_for(_capslock, capslockcodes, kc, i);
			look_for(_modeswitch, modeswitchcodes, kc, i);
		}
	}

	::free(modmap_r);

	::free(numlockcodes);
	::free(shiftlockcodes);
	::free(capslockcodes);
	::free(modeswitchcodes);

	// only if no xkb available
	if (!_xkb) {
		memset(_keycodes, 0, sizeof(InputKeyCode) * 256);
		// from https://stackoverflow.com/questions/18689863/obtain-keyboard-layout-and-keysyms-with-xcb
		xcb_get_keyboard_mapping_reply_t *keyboard_mapping = _xcb->xcb_get_keyboard_mapping_reply(_connection, mappingCookie, NULL);

		int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;

		xcb_keysym_t *keysyms = (xcb_keysym_t*) (keyboard_mapping + 1);

		for (int keycode_idx = 0; keycode_idx < nkeycodes; ++keycode_idx) {
			_keycodes[setup->min_keycode + keycode_idx] = getKeysymCode(keysyms[keycode_idx * keyboard_mapping->keysyms_per_keycode]);
		}

		free(keyboard_mapping);
	}
}

xcb_keysym_t XcbView::getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) {
	xcb_keysym_t k0, k1;

	if (!resolveMods) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 0);
		// resolve only numlock
		if ((state & _numlock)) {
			k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 1);
			if (_xcb->xcb_is_keypad_key(k1)) {
				if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
					return k0;
				} else {
					return k1;
				}
			}
		}
		return k0;
	}

	if (state & _modeswitch) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 2);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 3);
	} else {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 0);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keysyms, code, 1);
	}

	if (k1 == XCB_NO_SYMBOL)
		k1 = k0;

	if ((state & _numlock) && _xcb->xcb_is_keypad_key(k1)) {
		if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
			return k0;
		} else {
			return k1;
		}
	} else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
		return k0;
	} else if (!(state & XCB_MOD_MASK_SHIFT) && ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		if (k0 >= XK_0 && k0 <= XK_9) {
			return k0;
		}
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT) && ((state & XCB_MOD_MASK_LOCK) && (state & _capslock))) {
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT) || ((state & XCB_MOD_MASK_LOCK) && (state & _shiftlock))) {
		return k1;
	}

	return XCB_NO_SYMBOL;
}

void XcbView::updateXkbKey(xcb_keycode_t code) {
	static const struct {
		InputKeyCode key;
		const char *name;
	} keymap[] = {
		{ InputKeyCode::GRAVE_ACCENT, "TLDE" },
		{ InputKeyCode::_1, "AE01" },
		{ InputKeyCode::_2, "AE02" },
		{ InputKeyCode::_3, "AE03" },
		{ InputKeyCode::_4, "AE04" },
		{ InputKeyCode::_5, "AE05" },
		{ InputKeyCode::_6, "AE06" },
		{ InputKeyCode::_7, "AE07" },
		{ InputKeyCode::_8, "AE08" },
		{ InputKeyCode::_9, "AE09" },
		{ InputKeyCode::_0, "AE10" },
		{ InputKeyCode::MINUS, "AE11" },
		{ InputKeyCode::EQUAL, "AE12" },
		{ InputKeyCode::Q, "AD01" },
		{ InputKeyCode::W, "AD02" },
		{ InputKeyCode::E, "AD03" },
		{ InputKeyCode::R, "AD04" },
		{ InputKeyCode::T, "AD05" },
		{ InputKeyCode::Y, "AD06" },
		{ InputKeyCode::U, "AD07" },
		{ InputKeyCode::I, "AD08" },
		{ InputKeyCode::O, "AD09" },
		{ InputKeyCode::P, "AD10" },
		{ InputKeyCode::LEFT_BRACKET, "AD11" },
		{ InputKeyCode::RIGHT_BRACKET, "AD12" },
		{ InputKeyCode::A, "AC01" },
		{ InputKeyCode::S, "AC02" },
		{ InputKeyCode::D, "AC03" },
		{ InputKeyCode::F, "AC04" },
		{ InputKeyCode::G, "AC05" },
		{ InputKeyCode::H, "AC06" },
		{ InputKeyCode::J, "AC07" },
		{ InputKeyCode::K, "AC08" },
		{ InputKeyCode::L, "AC09" },
		{ InputKeyCode::SEMICOLON, "AC10" },
		{ InputKeyCode::APOSTROPHE, "AC11" },
		{ InputKeyCode::Z, "AB01" },
		{ InputKeyCode::X, "AB02" },
		{ InputKeyCode::C, "AB03" },
		{ InputKeyCode::V, "AB04" },
		{ InputKeyCode::B, "AB05" },
		{ InputKeyCode::N, "AB06" },
		{ InputKeyCode::M, "AB07" },
		{ InputKeyCode::COMMA, "AB08" },
		{ InputKeyCode::PERIOD, "AB09" },
		{ InputKeyCode::SLASH, "AB10" },
		{ InputKeyCode::BACKSLASH, "BKSL" },
		{ InputKeyCode::WORLD_1, "LSGT" },
		{ InputKeyCode::SPACE, "SPCE" },
		{ InputKeyCode::ESCAPE, "ESC" },
		{ InputKeyCode::ENTER, "RTRN" },
		{ InputKeyCode::TAB, "TAB" },
		{ InputKeyCode::BACKSPACE, "BKSP" },
		{ InputKeyCode::INSERT, "INS" },
		{ InputKeyCode::DELETE, "DELE" },
		{ InputKeyCode::RIGHT, "RGHT" },
		{ InputKeyCode::LEFT, "LEFT" },
		{ InputKeyCode::DOWN, "DOWN" },
		{ InputKeyCode::UP, "UP" },
		{ InputKeyCode::PAGE_UP, "PGUP" },
		{ InputKeyCode::PAGE_DOWN, "PGDN" },
		{ InputKeyCode::HOME, "HOME" },
		{ InputKeyCode::END, "END" },
		{ InputKeyCode::CAPS_LOCK, "CAPS" },
		{ InputKeyCode::SCROLL_LOCK, "SCLK" },
		{ InputKeyCode::NUM_LOCK, "NMLK" },
		{ InputKeyCode::PRINT_SCREEN, "PRSC" },
		{ InputKeyCode::PAUSE, "PAUS" },
		{ InputKeyCode::F1, "FK01" },
		{ InputKeyCode::F2, "FK02" },
		{ InputKeyCode::F3, "FK03" },
		{ InputKeyCode::F4, "FK04" },
		{ InputKeyCode::F5, "FK05" },
		{ InputKeyCode::F6, "FK06" },
		{ InputKeyCode::F7, "FK07" },
		{ InputKeyCode::F8, "FK08" },
		{ InputKeyCode::F9, "FK09" },
		{ InputKeyCode::F10, "FK10" },
		{ InputKeyCode::F11, "FK11" },
		{ InputKeyCode::F12, "FK12" },
		{ InputKeyCode::F13, "FK13" },
		{ InputKeyCode::F14, "FK14" },
		{ InputKeyCode::F15, "FK15" },
		{ InputKeyCode::F16, "FK16" },
		{ InputKeyCode::F17, "FK17" },
		{ InputKeyCode::F18, "FK18" },
		{ InputKeyCode::F19, "FK19" },
		{ InputKeyCode::F20, "FK20" },
		{ InputKeyCode::F21, "FK21" },
		{ InputKeyCode::F22, "FK22" },
		{ InputKeyCode::F23, "FK23" },
		{ InputKeyCode::F24, "FK24" },
		{ InputKeyCode::F25, "FK25" },
		{ InputKeyCode::KP_0, "KP0" },
		{ InputKeyCode::KP_1, "KP1" },
		{ InputKeyCode::KP_2, "KP2" },
		{ InputKeyCode::KP_3, "KP3" },
		{ InputKeyCode::KP_4, "KP4" },
		{ InputKeyCode::KP_5, "KP5" },
		{ InputKeyCode::KP_6, "KP6" },
		{ InputKeyCode::KP_7, "KP7" },
		{ InputKeyCode::KP_8, "KP8" },
		{ InputKeyCode::KP_9, "KP9" },
		{ InputKeyCode::KP_DECIMAL, "KPDL" },
		{ InputKeyCode::KP_DIVIDE, "KPDV" },
		{ InputKeyCode::KP_MULTIPLY, "KPMU" },
		{ InputKeyCode::KP_SUBTRACT, "KPSU" },
		{ InputKeyCode::KP_ADD, "KPAD" },
		{ InputKeyCode::KP_ENTER, "KPEN" },
		{ InputKeyCode::KP_EQUAL, "KPEQ" },
		{ InputKeyCode::LEFT_SHIFT, "LFSH" },
		{ InputKeyCode::LEFT_CONTROL, "LCTL" },
		{ InputKeyCode::LEFT_ALT, "LALT" },
		{ InputKeyCode::LEFT_SUPER, "LWIN" },
		{ InputKeyCode::RIGHT_SHIFT, "RTSH" },
		{ InputKeyCode::RIGHT_CONTROL, "RCTL" },
		{ InputKeyCode::RIGHT_ALT, "RALT" },
		{ InputKeyCode::RIGHT_ALT, "LVL3" },
		{ InputKeyCode::RIGHT_ALT, "MDSW" },
		{ InputKeyCode::RIGHT_SUPER, "RWIN" },
		{ InputKeyCode::MENU, "MENU" }
	};

	InputKeyCode key = InputKeyCode::Unknown;
	if (auto name = _xkb->xkb_keymap_key_get_name(_xkbKeymap, code)) {
		for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
			if (strncmp(name, keymap[i].name, 4) == 0) {
				key = keymap[i].key;
				break;
			}
		}
	}

	if (key != InputKeyCode::Unknown) {
		_keycodes[code] = key;
	}
}

InputKeyCode XcbView::getKeyCode(xcb_keycode_t code) const {
	return _keycodes[code];
}

InputKeyCode XcbView::getKeysymCode(xcb_keysym_t sym) const {
	// from GLFW: x11_init.c
	switch (sym) {
	case XK_KP_0: return InputKeyCode::KP_0;
	case XK_KP_1: return InputKeyCode::KP_1;
	case XK_KP_2: return InputKeyCode::KP_2;
	case XK_KP_3: return InputKeyCode::KP_3;
	case XK_KP_4: return InputKeyCode::KP_4;
	case XK_KP_5: return InputKeyCode::KP_5;
	case XK_KP_6: return InputKeyCode::KP_6;
	case XK_KP_7: return InputKeyCode::KP_7;
	case XK_KP_8: return InputKeyCode::KP_8;
	case XK_KP_9: return InputKeyCode::KP_9;
	case XK_KP_Separator:
	case XK_KP_Decimal: return InputKeyCode::KP_DECIMAL;
	case XK_Escape: return InputKeyCode::ESCAPE;
	case XK_Tab: return InputKeyCode::TAB;
	case XK_Shift_L: return InputKeyCode::LEFT_SHIFT;
	case XK_Shift_R: return InputKeyCode::RIGHT_SHIFT;
	case XK_Control_L: return InputKeyCode::LEFT_CONTROL;
	case XK_Control_R: return InputKeyCode::RIGHT_CONTROL;
	case XK_Meta_L:
	case XK_Alt_L: return InputKeyCode::LEFT_ALT;
	case XK_Mode_switch: // Mapped to Alt_R on many keyboards
	case XK_ISO_Level3_Shift: // AltGr on at least some machines
	case XK_Meta_R:
	case XK_Alt_R: return InputKeyCode::RIGHT_ALT;
	case XK_Super_L: return InputKeyCode::LEFT_SUPER;
	case XK_Super_R: return InputKeyCode::RIGHT_SUPER;
	case XK_Menu: return InputKeyCode::MENU;
	case XK_Num_Lock: return InputKeyCode::NUM_LOCK;
	case XK_Caps_Lock: return InputKeyCode::CAPS_LOCK;
	case XK_Print: return InputKeyCode::PRINT_SCREEN;
	case XK_Scroll_Lock: return InputKeyCode::SCROLL_LOCK;
	case XK_Pause: return InputKeyCode::PAUSE;
	case XK_Delete: return InputKeyCode::DELETE;
	case XK_BackSpace: return InputKeyCode::BACKSPACE;
	case XK_Return: return InputKeyCode::ENTER;
	case XK_Home: return InputKeyCode::HOME;
	case XK_End: return InputKeyCode::END;
	case XK_Page_Up: return InputKeyCode::PAGE_UP;
	case XK_Page_Down: return InputKeyCode::PAGE_DOWN;
	case XK_Insert: return InputKeyCode::INSERT;
	case XK_Left: return InputKeyCode::LEFT;
	case XK_Right: return InputKeyCode::RIGHT;
	case XK_Down: return InputKeyCode::DOWN;
	case XK_Up: return InputKeyCode::UP;
	case XK_F1: return InputKeyCode::F1;
	case XK_F2: return InputKeyCode::F2;
	case XK_F3: return InputKeyCode::F3;
	case XK_F4: return InputKeyCode::F4;
	case XK_F5: return InputKeyCode::F5;
	case XK_F6: return InputKeyCode::F6;
	case XK_F7: return InputKeyCode::F7;
	case XK_F8: return InputKeyCode::F8;
	case XK_F9: return InputKeyCode::F9;
	case XK_F10: return InputKeyCode::F10;
	case XK_F11: return InputKeyCode::F11;
	case XK_F12: return InputKeyCode::F12;
	case XK_F13: return InputKeyCode::F13;
	case XK_F14: return InputKeyCode::F14;
	case XK_F15: return InputKeyCode::F15;
	case XK_F16: return InputKeyCode::F16;
	case XK_F17: return InputKeyCode::F17;
	case XK_F18: return InputKeyCode::F18;
	case XK_F19: return InputKeyCode::F19;
	case XK_F20: return InputKeyCode::F20;
	case XK_F21: return InputKeyCode::F21;
	case XK_F22: return InputKeyCode::F22;
	case XK_F23: return InputKeyCode::F23;
	case XK_F24: return InputKeyCode::F24;
	case XK_F25: return InputKeyCode::F25;

		// Numeric keypad
	case XK_KP_Divide: return InputKeyCode::KP_DIVIDE;
	case XK_KP_Multiply: return InputKeyCode::KP_MULTIPLY;
	case XK_KP_Subtract: return InputKeyCode::KP_SUBTRACT;
	case XK_KP_Add: return InputKeyCode::KP_ADD;

		// These should have been detected in secondary keysym test above!
	case XK_KP_Insert: return InputKeyCode::KP_0;
	case XK_KP_End: return InputKeyCode::KP_1;
	case XK_KP_Down: return InputKeyCode::KP_2;
	case XK_KP_Page_Down: return InputKeyCode::KP_3;
	case XK_KP_Left: return InputKeyCode::KP_4;
	case XK_KP_Right: return InputKeyCode::KP_6;
	case XK_KP_Home: return InputKeyCode::KP_7;
	case XK_KP_Up: return InputKeyCode::KP_8;
	case XK_KP_Page_Up: return InputKeyCode::KP_9;
	case XK_KP_Delete: return InputKeyCode::KP_DECIMAL;
	case XK_KP_Equal: return InputKeyCode::KP_EQUAL;
	case XK_KP_Enter: return InputKeyCode::KP_ENTER;

		// Last resort: Check for printable keys (should not happen if the XKB
		// extension is available). This will give a layout dependent mapping
		// (which is wrong, and we may miss some keys, especially on non-US
		// keyboards), but it's better than nothing...
	case XK_a: return InputKeyCode::A;
	case XK_b: return InputKeyCode::B;
	case XK_c: return InputKeyCode::C;
	case XK_d: return InputKeyCode::D;
	case XK_e: return InputKeyCode::E;
	case XK_f: return InputKeyCode::F;
	case XK_g: return InputKeyCode::G;
	case XK_h: return InputKeyCode::H;
	case XK_i: return InputKeyCode::I;
	case XK_j: return InputKeyCode::J;
	case XK_k: return InputKeyCode::K;
	case XK_l: return InputKeyCode::L;
	case XK_m: return InputKeyCode::M;
	case XK_n: return InputKeyCode::N;
	case XK_o: return InputKeyCode::O;
	case XK_p: return InputKeyCode::P;
	case XK_q: return InputKeyCode::Q;
	case XK_r: return InputKeyCode::R;
	case XK_s: return InputKeyCode::S;
	case XK_t: return InputKeyCode::T;
	case XK_u: return InputKeyCode::U;
	case XK_v: return InputKeyCode::V;
	case XK_w: return InputKeyCode::W;
	case XK_x: return InputKeyCode::X;
	case XK_y: return InputKeyCode::Y;
	case XK_z: return InputKeyCode::Z;
	case XK_1: return InputKeyCode::_1;
	case XK_2: return InputKeyCode::_2;
	case XK_3: return InputKeyCode::_3;
	case XK_4: return InputKeyCode::_4;
	case XK_5: return InputKeyCode::_5;
	case XK_6: return InputKeyCode::_6;
	case XK_7: return InputKeyCode::_7;
	case XK_8: return InputKeyCode::_8;
	case XK_9: return InputKeyCode::_9;
	case XK_0: return InputKeyCode::_0;
	case XK_space: return InputKeyCode::SPACE;
	case XK_minus: return InputKeyCode::MINUS;
	case XK_equal: return InputKeyCode::EQUAL;
	case XK_bracketleft: return InputKeyCode::LEFT_BRACKET;
	case XK_bracketright: return InputKeyCode::RIGHT_BRACKET;
	case XK_backslash: return InputKeyCode::BACKSLASH;
	case XK_semicolon: return InputKeyCode::SEMICOLON;
	case XK_apostrophe: return InputKeyCode::APOSTROPHE;
	case XK_grave: return InputKeyCode::GRAVE_ACCENT;
	case XK_comma: return InputKeyCode::COMMA;
	case XK_period: return InputKeyCode::PERIOD;
	case XK_slash: return InputKeyCode::SLASH;
	case XK_less: return InputKeyCode::WORLD_1; // At least in some layouts...
	default: break;
	}
	return InputKeyCode::Unknown;
}

}

#endif // LINUX && LINUX_XCB
