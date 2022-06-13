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
#include "XLPlatformLinux.h"
#include "XLInputDispatcher.h"

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

	_socket = _xcb->xcb_get_file_descriptor(_connection); // assume it's non-blocking

	uint32_t mask = /*XCB_CW_BACK_PIXEL | */ XCB_CW_EVENT_MASK;
	uint32_t values[1];
	//values[0] = _defaultScreen->white_pixel;
	values[0] = // XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |	XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
			| XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_COLOR_MAP_CHANGE
			| XCB_EVENT_MASK_OWNER_GRAB_BUTTON;
	// XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_RESIZE_REDIRECT

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

	_xcb->xcb_map_window(_connection, _window);

    xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

    size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom( _connection, it.onlyIfExists ? 1 : 0, it.name.size(), it.name.data() );
		++ i;
	}

	_xcb->xcb_flush(_connection);

    i = 0;
	for (auto &it : atomCookies) {
		auto reply = _xcb->xcb_intern_atom_reply( _connection, it, nullptr );
		if (reply) {
			_atoms[i] = reply->atom;
			free(reply);
		} else {
			_atoms[i] = 0;
		}
		++ i;
	}

	_xcb->xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, _atoms[0], 4, 32, 1, &_atoms[1] );
	_xcb->xcb_flush(_connection);

	impl->setScreenExtent(Extent2(_width, _height));
}

XcbView::~XcbView() {
	_defaultScreen = nullptr;
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
			InputModifier::Mod2, InputModifier::Mod3, InputModifier::Mod4, InputModifier::Mod5,
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

static void print_modifiers (uint32_t mask) {
	const char **mod, *mods[] = { "Shift", "Lock", "Ctrl", "Alt", "Mod2", "Mod3",
			"Mod4", "Mod5", "Button1", "Button2", "Button3", "Button4", "Button5" };
	printf("Modifier mask: ");
	for (mod = mods; mask; mask >>= 1, mod++) {
		if (mask & 1) {
			printf("%s", *mod);
		}
	}
	putchar('\n');
}

bool XcbView::poll() {
	bool ret = true;
	bool deprecateSwapchain = false;

	_view->getDevice()->makeApiCall([&] (const vk::DeviceTable &, VkDevice) {
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
					auto ext = _view->getScreenExtent();
					auto mod = getModifiers(ev->state);
					auto btn = getButton(ev->detail);

					InputEventData event({
						ev->detail,
						InputEventName::Begin,
						float(ev->event_x),
						float(ext.height - ev->event_y),
						btn,
						mod
					});

					switch (btn) {
					case InputMouseButton::MouseScrollUp:
						event.event = InputEventName::Scroll;
						event.valueX = 0.0f; event.valueY = 1.0f;
						break;
					case InputMouseButton::MouseScrollDown:
						event.event = InputEventName::Scroll;
						event.valueX = 0.0f; event.valueY = -1.0f;
						break;
					case InputMouseButton::MouseScrollLeft:
						event.event = InputEventName::Scroll;
						event.valueX = 1.0f; event.valueY = 0.0f;
						break;
					case InputMouseButton::MouseScrollRight:
						event.event = InputEventName::Scroll;
						event.valueX = -1.0f; event.valueY = 0.0f;
						break;
					default:
						break;
					}

					_view->handleInputEvent(event);
				}
				break;
			case XCB_BUTTON_RELEASE:
				if (_window == ((xcb_button_release_event_t *)e)->event) {
					auto ev = (xcb_button_release_event_t *)e;
					auto ext = _view->getScreenExtent();
					auto mod = getModifiers(ev->state);
					auto btn = getButton(ev->detail);

					InputEventData event({
						ev->detail,
						InputEventName::End,
						float(ev->event_x),
						float(ext.height - ev->event_y),
						btn,
						mod
					});

					switch (btn) {
					case InputMouseButton::MouseScrollUp:
					case InputMouseButton::MouseScrollDown:
					case InputMouseButton::MouseScrollLeft:
					case InputMouseButton::MouseScrollRight:
						break;
					default:
						_view->handleInputEvent(event);
						break;
					}
				}
				break;
			case XCB_MOTION_NOTIFY:
				if (_window == ((xcb_motion_notify_event_t *)e)->event) {
					auto ev = (xcb_motion_notify_event_t *)e;
					auto ext = _view->getScreenExtent();
					auto mod = getModifiers(ev->state);

					InputEventData event({
						maxOf<uint32_t>(),
						InputEventName::MouseMove,
						float(ev->event_x),
						float(ext.height - ev->event_y),
						InputMouseButton::None,
						mod
					});

					_view->handleInputEvent(event);
				}
				break;
			case XCB_ENTER_NOTIFY: {
				xcb_enter_notify_event_t *ev = (xcb_enter_notify_event_t*) e;
				_view->handleInputEvent(InputEventData::BoolEvent(InputEventName::PointerEnter, true));
				printf("Mouse entered window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
				break;
			}
			case XCB_LEAVE_NOTIFY: {
				xcb_leave_notify_event_t *ev = (xcb_leave_notify_event_t*) e;
				_view->handleInputEvent(InputEventData::BoolEvent(InputEventName::PointerEnter, false));
				printf("Mouse left window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
				break;
			}
			case XCB_FOCUS_IN: {
				xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
				_view->handleInputEvent(InputEventData::BoolEvent(InputEventName::FocusGain, true));
				printf("XCB_FOCUS_IN: %d\n", ev->event);
				break;
			}
			case XCB_FOCUS_OUT: {
				xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
				_view->handleInputEvent(InputEventData::BoolEvent(InputEventName::FocusGain, false));
				printf("XCB_FOCUS_OUT: %d\n", ev->event);
				break;
			}
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *ev = (xcb_key_press_event_t*) e;
				print_modifiers(ev->state);
				printf("Key pressed in window %d\n", ev->event);
				break;
			}
			case XCB_KEY_RELEASE: {
				xcb_key_release_event_t *ev = (xcb_key_release_event_t*) e;
				print_modifiers(ev->state);
				printf("Key released in window %d\n", ev->event);
				break;
			}
			case XCB_VISIBILITY_NOTIFY: {
				xcb_visibility_notify_event_t *ev = (xcb_visibility_notify_event_t*) e;
				printf("XCB_VISIBILITY_NOTIFY: %d\n", ev->window);
				break;
			}
			case XCB_MAP_NOTIFY : {
				xcb_map_notify_event_t *ev = (xcb_map_notify_event_t*) e;
				printf("XCB_MAP_NOTIFY: %d\n", ev->event);
				break;
			}
			case XCB_REPARENT_NOTIFY : {
				xcb_reparent_notify_event_t *ev = (xcb_reparent_notify_event_t*) e;
				printf("XCB_REPARENT_NOTIFY: %d %d to %d\n", ev->event, ev->window, ev->parent);
				break;
			}
			case XCB_CONFIGURE_NOTIFY : {
				xcb_configure_notify_event_t *ev = (xcb_configure_notify_event_t*) e;
				// printf("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event, ev->window,
				//		ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width), uint32_t(ev->override_redirect));
				if (ev->width != _width || ev->height != _height) {
					_width = ev->width; _height = ev->height;
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
	        case XCB_PROPERTY_NOTIFY : {
	        	// xcb_property_notify_event_t *ev = (xcb_property_notify_event_t*) e;
				// printf("XCB_PROPERTY_NOTIFY: %d of property %d\n", ev->window, ev->atom);
				break;
	        }
	        case XCB_MAPPING_NOTIFY : {
	        	xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t*) e;
				printf("XCB_MAPPING_NOTIFY: %d %d %d\n", (int)ev->request, (int)ev->first_keycode, (int)ev->count);
				break;
	        }
			default:
				/* Unknown event type, ignore it */
				printf("Unknown event: %d\n", et);
				break;
			}

			/* Free the Generic Event */
			free(e);
		}
	});

	if (deprecateSwapchain) {
		_view->deprecateSwapchain();
	}

	return ret;
}

}

#endif // LINUX && LINUX_XCB
