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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXWAYLAND_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXWAYLAND_H_

#include "XLPlatformLinux.h"

#if LINUX

#include "wayland-protocols/xdg-shell-client-protocol.h"

#include <wayland-client.h>

namespace stappler::xenolith::platform {

struct XdgInterface {
	const struct wl_interface *xdg_shell_types[26];

	const struct wl_message xdg_wm_base_requests[4];
	const struct wl_message xdg_wm_base_events[1];
	const struct wl_message xdg_positioner_requests[10];
	const struct wl_message xdg_surface_requests[5];
	const struct wl_message xdg_surface_events[1];
	const struct wl_message xdg_toplevel_requests[14];
	const struct wl_message xdg_toplevel_events[3];
	const struct wl_message xdg_popup_requests[3];
	const struct wl_message xdg_popup_events[3];

	const struct wl_interface xdg_wm_base_interface;
	const struct wl_interface xdg_positioner_interface;
	const struct wl_interface xdg_surface_interface;
	const struct wl_interface xdg_toplevel_interface;
	const struct wl_interface xdg_popup_interface;

	XdgInterface(const struct wl_interface *wl_output_interface, const struct wl_interface *wl_seat_interface,
			const struct wl_interface *wl_surface_interface)
	: xdg_shell_types{
		NULL,
		NULL,
		NULL,
		NULL,
		&xdg_positioner_interface,
		&xdg_surface_interface,
		wl_surface_interface,
		&xdg_toplevel_interface,
		&xdg_popup_interface,
		&xdg_surface_interface,
		&xdg_positioner_interface,
		&xdg_toplevel_interface,
		wl_seat_interface,
		NULL,
		NULL,
		NULL,
		wl_seat_interface,
		NULL,
		wl_seat_interface,
		NULL,
		NULL,
		wl_output_interface,
		wl_seat_interface,
		NULL,
		&xdg_positioner_interface,
		NULL,
	}, xdg_wm_base_requests{
		{ "destroy", "", xdg_shell_types + 0 },
		{ "create_positioner", "n", xdg_shell_types + 4 },
		{ "get_xdg_surface", "no", xdg_shell_types + 5 },
		{ "pong", "u", xdg_shell_types + 0 },
	}, xdg_wm_base_events{
		{ "ping", "u", xdg_shell_types + 0 },
	}, xdg_positioner_requests{
		{ "destroy", "", xdg_shell_types + 0 },
		{ "set_size", "ii", xdg_shell_types + 0 },
		{ "set_anchor_rect", "iiii", xdg_shell_types + 0 },
		{ "set_anchor", "u", xdg_shell_types + 0 },
		{ "set_gravity", "u", xdg_shell_types + 0 },
		{ "set_constraint_adjustment", "u", xdg_shell_types + 0 },
		{ "set_offset", "ii", xdg_shell_types + 0 },
		{ "set_reactive", "3", xdg_shell_types + 0 },
		{ "set_parent_size", "3ii", xdg_shell_types + 0 },
		{ "set_parent_configure", "3u", xdg_shell_types + 0 },
	}, xdg_surface_requests{
		{ "destroy", "", xdg_shell_types + 0 },
		{ "get_toplevel", "n", xdg_shell_types + 7 },
		{ "get_popup", "n?oo", xdg_shell_types + 8 },
		{ "set_window_geometry", "iiii", xdg_shell_types + 0 },
		{ "ack_configure", "u", xdg_shell_types + 0 },
	}, xdg_surface_events{
		{ "configure", "u", xdg_shell_types + 0 },
	}, xdg_toplevel_requests{
		{ "destroy", "", xdg_shell_types + 0 },
		{ "set_parent", "?o", xdg_shell_types + 11 },
		{ "set_title", "s", xdg_shell_types + 0 },
		{ "set_app_id", "s", xdg_shell_types + 0 },
		{ "show_window_menu", "ouii", xdg_shell_types + 12 },
		{ "move", "ou", xdg_shell_types + 16 },
		{ "resize", "ouu", xdg_shell_types + 18 },
		{ "set_max_size", "ii", xdg_shell_types + 0 },
		{ "set_min_size", "ii", xdg_shell_types + 0 },
		{ "set_maximized", "", xdg_shell_types + 0 },
		{ "unset_maximized", "", xdg_shell_types + 0 },
		{ "set_fullscreen", "?o", xdg_shell_types + 21 },
		{ "unset_fullscreen", "", xdg_shell_types + 0 },
		{ "set_minimized", "", xdg_shell_types + 0 },
	}, xdg_toplevel_events{
		{ "configure", "iia", xdg_shell_types + 0 },
		{ "close", "", xdg_shell_types + 0 },
		{ "configure_bounds", "4ii", xdg_shell_types + 0 },
	}, xdg_popup_requests{
		{ "destroy", "", xdg_shell_types + 0 },
		{ "grab", "ou", xdg_shell_types + 22 },
		{ "reposition", "3ou", xdg_shell_types + 24 },
	}, xdg_popup_events{
		{ "configure", "iiii", xdg_shell_types + 0 },
		{ "popup_done", "", xdg_shell_types + 0 },
		{ "repositioned", "3u", xdg_shell_types + 0 },
	}, xdg_wm_base_interface{
		"xdg_wm_base", 4,
		4, xdg_wm_base_requests,
		1, xdg_wm_base_events,
	}, xdg_positioner_interface{
		"xdg_positioner", 4,
		10, xdg_positioner_requests,
		0, NULL,
	}, xdg_surface_interface{
		"xdg_surface", 4,
		5, xdg_surface_requests,
		1, xdg_surface_events,
	}, xdg_toplevel_interface{
		"xdg_toplevel", 4,
		14, xdg_toplevel_requests,
		3, xdg_toplevel_events,
	}, xdg_popup_interface{
		"xdg_popup", 4,
		3, xdg_popup_requests,
		3, xdg_popup_events,
	} { }
};

class WaylandLibrary : public Ref {
public:
	struct ConnectionData {
		struct wl_display *display = nullptr;
	};

	static WaylandLibrary *getInstance();

	virtual ~WaylandLibrary();
	WaylandLibrary();

	bool init();
	void close();

	const struct wl_interface *wl_registry_interface = nullptr;
	const struct wl_interface *wl_compositor_interface = nullptr;
	const struct wl_interface *wl_output_interface = nullptr;
	const struct wl_interface *wl_seat_interface = nullptr;
	const struct wl_interface *wl_surface_interface = nullptr;
	const struct wl_interface *wl_region_interface = nullptr;
	const struct wl_interface *wl_callback_interface = nullptr;

	const struct wl_interface *xdg_wm_base_interface = nullptr;
	const struct wl_interface *xdg_positioner_interface = nullptr;
	const struct wl_interface *xdg_surface_interface = nullptr;
	const struct wl_interface *xdg_toplevel_interface = nullptr;
	const struct wl_interface *xdg_popup_interface = nullptr;

	struct wl_display * (* wl_display_connect) (const char *name) = nullptr;
	int (* wl_display_get_fd) (struct wl_display *display) = nullptr;
	int (* wl_display_dispatch) (struct wl_display *display) = nullptr;
	int (* wl_display_dispatch_pending) (struct wl_display *display) = nullptr;
	int (* wl_display_prepare_read) (struct wl_display *display) = nullptr;
	int (* wl_display_flush) (struct wl_display *display) = nullptr;
	int (* wl_display_read_events) (struct wl_display *display) = nullptr;
	void (* wl_display_disconnect) (struct wl_display *display) = nullptr;

	struct wl_proxy* (* wl_proxy_marshal_flags) (struct wl_proxy *proxy, uint32_t opcode,
			const struct wl_interface *interface, uint32_t version, uint32_t flags, ...) = nullptr;
	uint32_t (* wl_proxy_get_version) (struct wl_proxy *proxy) = nullptr;
	int (* wl_proxy_add_listener) (struct wl_proxy *proxy, void (**implementation)(void), void *data) = nullptr;
	void (* wl_proxy_destroy) (struct wl_proxy *proxy) = nullptr;
	int (* wl_display_roundtrip) (struct wl_display *display) = nullptr;

	XdgInterface *xdg = nullptr;

	struct wl_registry *wl_display_get_registry(struct wl_display *wl_display) {
		struct wl_proxy *registry;

		registry = wl_proxy_marshal_flags((struct wl_proxy*) wl_display, WL_DISPLAY_GET_REGISTRY,
				wl_registry_interface, wl_proxy_get_version((struct wl_proxy*) wl_display), 0, NULL);

		return (struct wl_registry*) registry;
	}

	void* wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_registry, WL_REGISTRY_BIND, interface, version,
				0, name, interface->name, version, NULL);

		return (void*) id;
	}

	int wl_registry_add_listener(struct wl_registry *wl_registry, const struct wl_registry_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_registry, (void (**)(void)) listener, data);
	}

	void wl_registry_destroy(struct wl_registry *wl_registry) {
		wl_proxy_destroy((struct wl_proxy*) wl_registry);
	}

	struct wl_surface *wl_compositor_create_surface(struct wl_compositor *wl_compositor) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_compositor, WL_COMPOSITOR_CREATE_SURFACE, wl_surface_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_compositor), 0, NULL);

		return (struct wl_surface*) id;
	}

	struct wl_region* wl_compositor_create_region(struct wl_compositor *wl_compositor) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_compositor, WL_COMPOSITOR_CREATE_REGION,
				wl_region_interface, wl_proxy_get_version((struct wl_proxy*) wl_compositor), 0, NULL);

		return (struct wl_region*) id;
	}

	void wl_compositor_destroy(struct wl_compositor *wl_compositor) {
		wl_proxy_destroy((struct wl_proxy *) wl_compositor);
	}

	void wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_region, WL_REGION_ADD, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_region), 0, x, y, width, height);
	}

	void wl_region_destroy(struct wl_region *wl_region) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_region, WL_REGION_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_region), WL_MARSHAL_FLAG_DESTROY);
	}

	void wl_surface_commit(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_COMMIT, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0);
	}

	void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_DAMAGE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, x, y, width, height);
	}

	void wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_SET_OPAQUE_REGION, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, region);
	}

	void wl_surface_destroy(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_callback_add_listener(struct wl_callback *wl_callback, const struct wl_callback_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_callback, (void (**)(void)) listener, data);
	}

	void wl_callback_destroy(struct wl_callback *wl_callback) {
		wl_proxy_destroy((struct wl_proxy*) wl_callback);
	}

	int wl_output_add_listener(struct wl_output *wl_output, const struct wl_output_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_output, (void (**)(void)) listener, data);
	}

	struct wl_callback* wl_surface_frame(struct wl_surface *wl_surface) {
		struct wl_proxy *callback;

		callback = wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_FRAME, wl_callback_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, NULL);

		return (struct wl_callback*) callback;
	}

	void wl_output_destroy(struct wl_output *wl_output) {
		wl_proxy_destroy((struct wl_proxy*) wl_output);
	}

	int xdg_wm_base_add_listener(struct xdg_wm_base *xdg_wm_base, const struct xdg_wm_base_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) xdg_wm_base, (void (**)(void)) listener, data);
	}

	void xdg_wm_base_pong(struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_wm_base, XDG_WM_BASE_PONG, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_wm_base), 0, serial);
	}

	void xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_surface, XDG_SURFACE_ACK_CONFIGURE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_surface), 0, serial);
	}

	struct xdg_surface* xdg_wm_base_get_xdg_surface(struct xdg_wm_base *xdg_wm_base, struct wl_surface *surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) xdg_wm_base,
		XDG_WM_BASE_GET_XDG_SURFACE, xdg_surface_interface, wl_proxy_get_version((struct wl_proxy*) xdg_wm_base), 0, NULL, surface);

		return (struct xdg_surface*) id;
	}

	int xdg_surface_add_listener(struct xdg_surface *xdg_surface, const struct xdg_surface_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) xdg_surface, (void (**)(void)) listener, data);
	}

	struct xdg_toplevel* xdg_surface_get_toplevel(struct xdg_surface *xdg_surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) xdg_surface, XDG_SURFACE_GET_TOPLEVEL,
				xdg_toplevel_interface, wl_proxy_get_version((struct wl_proxy*) xdg_surface), 0, NULL);

		return (struct xdg_toplevel*) id;
	}

	void xdg_surface_destroy(struct xdg_surface *xdg_surface) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_surface, XDG_SURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_surface), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_toplevel_set_title(struct xdg_toplevel *xdg_toplevel, const char *title) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, title);
	}

	void xdg_toplevel_set_app_id(struct xdg_toplevel *xdg_toplevel, const char *app_id) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_APP_ID, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, app_id);
	}

	int xdg_toplevel_add_listener(struct xdg_toplevel *xdg_toplevel, const struct xdg_toplevel_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) xdg_toplevel, (void (**)(void)) listener, data);
	}

	void xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_wm_base_destroy(struct xdg_wm_base *xdg_wm_base) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_wm_base, XDG_WM_BASE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_wm_base), WL_MARSHAL_FLAG_DESTROY);
	}

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection() const;

protected:
	bool open(void *);
	void openConnection(ConnectionData &data);

	void *_handle = nullptr;

	ConnectionData _pending;
	ConnectionData _current;
};

class WaylandView : public LinuxViewInterface {
public:
	WaylandView(WaylandLibrary *, ViewImpl *, StringView, URect);
	virtual ~WaylandView();

	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const override;

	virtual bool poll() override;

	virtual int getSocketFd() const override;

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

	const Rc<WaylandLibrary> &getWayland() const { return _wayland; }

	void handleGlobal(struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
	void handleGlobalRemove(struct wl_registry *registry, uint32_t name);

	void handleOutputGeometry(wl_output *output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
			int32_t subpixel, char const *make, char const *model, int32_t transform);
	void handleOutputMode(wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);
	void handleOutputDone(wl_output *output);
	void handleOutputScale(wl_output *output, int32_t factor);

	void handleSurfaceConfigure(xdg_surface *, uint32_t serial);
	void handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states);
	void handleToplevelClose(xdg_toplevel *xdg_toplevel);
	void handleSurfaceFrameDone(wl_callback *wl_callback, uint32_t callback_data);

	virtual void onSurfaceInfo(gl::SurfaceInfo &) const override;

protected:
	bool flush();

	Rc<WaylandLibrary> _wayland;
	ViewImpl *_view = nullptr;

	wl_display *_display = nullptr;
	wl_compositor *_compositor = nullptr;
	wl_output *_output = nullptr;
	struct xdg_wm_base *_xdgWmBase = nullptr;
	wl_surface *_surface = nullptr;
	xdg_surface *_xdgSurface = nullptr;
	xdg_toplevel *_toplevel = nullptr;
	Extent2 _currentExtent;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXWAYLAND_H_ */
