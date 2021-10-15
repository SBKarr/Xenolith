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

#if LINUX && LINUX_XCB

#include "XLVkDevice.h"
#include "XLGlSwapchain.h"

#include <sys/eventfd.h>

namespace stappler::xenolith::vk {

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

	XcbView(const Instance *, ViewImpl *, StringView, URect);
	virtual ~XcbView();

	bool valid() const;

	virtual bool isAvailableOnDevice(const Device &) const override;
	virtual VkSurfaceKHR createWindowSurface() override;

	virtual bool poll() override;

	virtual void onEventPushed() override;

	virtual int getEventFd() const override { return _eventFd; }
	virtual int getSocketFd() const override { return _socket; }

protected:
	const Instance *_instance = nullptr;
	ViewImpl *_view = nullptr;
	// Rc<gl::Loop> _loop;
	xcb_connection_t *_connection = nullptr;
	xcb_screen_t *_defaultScreen = nullptr;
	uint32_t _window = 0;

	xcb_atom_t _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

	uint16_t _width = 0;
	uint16_t _height = 0;

	int _eventFd = -1;
	int _socket = -1;
};


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

XcbView::XcbView(const Instance *inst, ViewImpl *v, StringView, URect rect) {
	_instance = inst;
	_view = v;
	if constexpr (s_printVkInfo) {
		auto d = getenv("DISPLAY");
		if (!d) {
			stappler::log::vtext("XcbView-Info", "DISPLAY is not defined");
		} else {
			// stappler::log::vtext("XcbView-Info", "Using DISPLAY: '", d, "'");
		}
	}

	auto connection = XcbConnectionCache::acquire();

	_connection = connection.connection;

	auto err = xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return;
	}

	_defaultScreen = connection.screen;

	_socket = xcb_get_file_descriptor(_connection); // assume it's non-blocking
	_eventFd = eventfd(0, EFD_NONBLOCK);

	uint32_t mask = /* XCB_CW_BACK_PIXEL | */ XCB_CW_EVENT_MASK;
	uint32_t values[1];
	//values[0] = _defaultScreen->black_pixel;
	values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		/*XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |	XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
			| XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_RESIZE_REDIRECT | XCB_EVENT_MASK_FOCUS_CHANGE
			| XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE |
			| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
						| XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
			| XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_RESIZE_REDIRECT  | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE
			| XCB_EVENT_MASK_COLOR_MAP_CHANGE | XCB_EVENT_MASK_OWNER_GRAB_BUTTON */;

	/* Ask for our window's Id */
	_window = xcb_generate_id(_connection);

	_width = rect.width;
	_height = rect.height;

	xcb_create_window(_connection,
		XCB_COPY_FROM_PARENT, // depth (same as root)
		_window, // window Id
		_defaultScreen->root, // parent window
		rect.x, rect.y, rect.width, rect.height,
		0, // border_width
		XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
		_defaultScreen->root_visual, // visual
		mask, values);

	xcb_map_window (_connection, _window);

    xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomRequest)];

    size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = xcb_intern_atom( _connection, it.onlyIfExists ? 1 : 0, it.name.size(), it.name.data() );
		++ i;
	}

	xcb_flush(_connection);

    i = 0;
	for (auto &it : atomCookies) {
		auto reply = xcb_intern_atom_reply( _connection, it, nullptr );
		if (reply) {
			_atoms[i] = reply->atom;
			free(reply);
		} else {
			_atoms[i] = 0;
		}
		++ i;
	}

	xcb_change_property( _connection, XCB_PROP_MODE_REPLACE, _window, _atoms[0], 4, 32, 1, &_atoms[1] );

	v->setScreenSize(_width, _height);
}

XcbView::~XcbView() {
	if (_eventFd) {
		close(_eventFd);
	}
	_defaultScreen = nullptr;
	if (_connection) {
		xcb_disconnect(_connection);
		_connection = nullptr;
	}
	_instance = nullptr;
}

bool XcbView::valid() const {
	return xcb_connection_has_error(_connection) == 0;
}

bool XcbView::isAvailableOnDevice(const Device &dev) const {
	return _instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(dev.getPhysicalDevice(),
			dev.getQueueFamily(QueueOperations::Graphics)->index, _connection, _defaultScreen->root_visual);
}

VkSurfaceKHR XcbView::createWindowSurface() {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkXcbSurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr, 0, _connection, _window};
	if (_instance->vkCreateXcbSurfaceKHR(_instance->getInstance(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
		return nullptr;
	}
	return surface;
}

void print_modifiers (uint32_t mask) {
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
	xcb_generic_event_t *e;
	while ((e = xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case XCB_EXPOSE: {
			xcb_expose_event_t *ev = (xcb_expose_event_t*) e;
			printf("XCB_EXPOSE: Window %d exposed. Region to be redrawn at location (%d,%d), with dimension (%d,%d)\n",
					ev->window, ev->x, ev->y, ev->width, ev->height);
			break;
		}
		case XCB_BUTTON_PRESS: {
			xcb_button_press_event_t *ev = (xcb_button_press_event_t*) e;
			print_modifiers(ev->state);

			switch (ev->detail) {
			case 4:
				printf("Wheel Button up in window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
				break;
			case 5:
				printf("Wheel Button down in window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
				break;
			default:
				printf("Button %d pressed in window %d, at coordinates (%d,%d)\n", ev->detail, ev->event, ev->event_x, ev->event_y);
			}
			break;
		}
		case XCB_BUTTON_RELEASE: {
			xcb_button_release_event_t *ev = (xcb_button_release_event_t*) e;
			print_modifiers(ev->state);
			printf("Button %d released in window %d, at coordinates (%d,%d)\n", ev->detail, ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_MOTION_NOTIFY: {
			// xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t*) e;
			//printf("Mouse moved in window %ld, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_ENTER_NOTIFY: {
			xcb_enter_notify_event_t *ev = (xcb_enter_notify_event_t*) e;
			printf("Mouse entered window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_LEAVE_NOTIFY: {
			xcb_leave_notify_event_t *ev = (xcb_leave_notify_event_t*) e;
			printf("Mouse left window %d, at coordinates (%d,%d)\n", ev->event, ev->event_x, ev->event_y);
			break;
		}
		case XCB_FOCUS_IN: {
			xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
			printf("XCB_FOCUS_IN: %d\n", ev->event);
			break;
		}
		case XCB_FOCUS_OUT: {
			xcb_focus_in_event_t *ev = (xcb_focus_in_event_t*) e;
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
			print_modifiers(ev->state);
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
			printf("XCB_CONFIGURE_NOTIFY: %d (%d) rect:%d,%d,%d,%d border:%d override:%d\n", ev->event, ev->window,
					ev->x, ev->y, ev->width, ev->height, uint32_t(ev->border_width), uint32_t(ev->override_redirect));
			if (ev->width != _width || ev->height != _height) {
				_width = ev->width; _height = ev->height;
				_view->recreateSwapChain();
				_view->setScreenSize(_width, _height);
			}
			break;
		}
        case XCB_CLIENT_MESSAGE: {
        	xcb_client_message_event_t *ev = (xcb_client_message_event_t*) e;
			printf("XCB_CLIENT_MESSAGE: %d of type %d\n", ev->window, ev->type);
			if (ev->type == _atoms[0] && ev->data.data32[0] == _atoms[1]) {
				free(e);
				return false;
			}
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
	return true;
}

void XcbView::onEventPushed() {
	uint64_t value = 1;
	write(_eventFd, &value, sizeof(uint64_t));
}

}

#endif // LINUX && LINUX_XCB
