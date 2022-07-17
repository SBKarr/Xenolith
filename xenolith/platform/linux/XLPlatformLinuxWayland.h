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
#include "wayland-protocols/viewporter-client-protocol.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <bitset>

namespace stappler::xenolith::platform {

class WaylandView;
struct WaylandShm;
struct WaylandSeat;
struct WaylandOutput;
struct WaylandDecoration;

struct XdgInterface;
struct ViewporterInterface;

class WaylandLibrary : public Ref {
public:
	struct ConnectionData {
		struct wl_display *display = nullptr;
	};

	static WaylandLibrary *getInstance();

	static bool ownsProxy(wl_proxy *);
	static bool ownsProxy(wl_output *);
	static bool ownsProxy(wl_surface *);

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
	const struct wl_interface *wl_pointer_interface = nullptr;
	const struct wl_interface *wl_keyboard_interface = nullptr;
	const struct wl_interface *wl_touch_interface = nullptr;
	const struct wl_interface *wl_shm_interface = nullptr;
	const struct wl_interface *wl_subcompositor_interface = nullptr;
	const struct wl_interface *wl_subsurface_interface = nullptr;
	const struct wl_interface *wl_shm_pool_interface = nullptr;
	const struct wl_interface *wl_buffer_interface = nullptr;

	const struct wl_interface *wp_viewporter_interface = nullptr;
	const struct wl_interface *wp_viewport_interface = nullptr;

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
	void (* wl_proxy_set_user_data) (struct wl_proxy *proxy, void *user_data) = nullptr;
	void *(* wl_proxy_get_user_data) (struct wl_proxy *proxy) = nullptr;
	void (* wl_proxy_set_tag) (struct wl_proxy *proxy, const char * const *tag) = nullptr;
	const char * const *(* wl_proxy_get_tag) (struct wl_proxy *proxy) = nullptr;
	void (* wl_proxy_destroy) (struct wl_proxy *proxy) = nullptr;
	int (* wl_display_roundtrip) (struct wl_display *display) = nullptr;

	ViewporterInterface *viewporter = nullptr;
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

	struct wl_shm_pool* wl_shm_create_pool(struct wl_shm *wl_shm, int32_t fd, int32_t size) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_shm, WL_SHM_CREATE_POOL, wl_shm_pool_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_shm), 0, NULL, fd, size);

		return (struct wl_shm_pool*) id;
	}

	int wl_shm_add_listener(struct wl_shm *wl_shm, const struct wl_shm_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_shm, (void (**)(void)) listener, data);
	}

	void wl_shm_set_user_data(struct wl_shm *wl_shm, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy*) wl_shm, user_data);
	}

	void* wl_shm_get_user_data(struct wl_shm *wl_shm) {
		return wl_proxy_get_user_data((struct wl_proxy*) wl_shm);
	}

	void wl_shm_destroy(struct wl_shm *wl_shm) {
		wl_proxy_destroy((struct wl_proxy*) wl_shm);
	}

	void wl_shm_pool_destroy(struct wl_shm_pool *wl_shm_pool) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_shm_pool, WL_SHM_POOL_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_shm_pool), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wl_buffer* wl_shm_pool_create_buffer(struct wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_shm_pool, WL_SHM_POOL_CREATE_BUFFER, wl_buffer_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_shm_pool), 0, NULL, offset, width, height, stride, format);

		return (struct wl_buffer*) id;
	}

	void wl_buffer_destroy(struct wl_buffer *wl_buffer) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_buffer, WL_BUFFER_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_buffer), WL_MARSHAL_FLAG_DESTROY);
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

	int wl_surface_add_listener(struct wl_surface *wl_surface, const struct wl_surface_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_surface, (void (**)(void)) listener, data);
	}

	void wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_ATTACH, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, buffer, x, y);
	}

	void wl_surface_set_buffer_scale(struct wl_surface *wl_surface, int32_t scale) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_SET_BUFFER_SCALE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, scale);
	}

	void wl_surface_commit(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_COMMIT, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0);
	}

	void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_DAMAGE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, x, y, width, height);
	}

	void wl_surface_damage_buffer(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_DAMAGE_BUFFER, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, x, y, width, height);
	}

	void wl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_SET_OPAQUE_REGION, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), 0, region);
	}

	void wl_surface_set_user_data(struct wl_surface *wl_surface, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy*) wl_surface, user_data);
	}

	void* wl_surface_get_user_data(struct wl_surface *wl_surface) {
		return wl_proxy_get_user_data((struct wl_proxy*) wl_surface);
	}

	void wl_surface_destroy(struct wl_surface *wl_surface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_surface, WL_SURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_surface), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wl_subsurface* wl_subcompositor_get_subsurface(struct wl_subcompositor *wl_subcompositor, struct wl_surface *surface, struct wl_surface *parent) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_subcompositor, WL_SUBCOMPOSITOR_GET_SUBSURFACE, wl_subsurface_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_subcompositor), 0, NULL, surface, parent);

		return (struct wl_subsurface*) id;
	}

	void wl_subcompositor_destroy(struct wl_subcompositor *wl_subcompositor) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subcompositor, WL_SUBCOMPOSITOR_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subcompositor), WL_MARSHAL_FLAG_DESTROY);
	}

	void wl_subsurface_set_position(struct wl_subsurface *wl_subsurface, int32_t x, int32_t y) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_SET_POSITION, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), 0, x, y);
	}

	void wl_subsurface_place_above(struct wl_subsurface *wl_subsurface, struct wl_surface *sibling) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_PLACE_ABOVE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), 0, sibling);
	}

	void wl_subsurface_place_below(struct wl_subsurface *wl_subsurface, struct wl_surface *sibling) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_PLACE_BELOW, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), 0, sibling);
	}

	void wl_subsurface_set_sync(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_SET_SYNC, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), 0);
	}

	void wl_subsurface_set_desync(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_SET_DESYNC, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), 0);
	}

	void wl_subsurface_destroy(struct wl_subsurface *wl_subsurface) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_subsurface, WL_SUBSURFACE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_subsurface), WL_MARSHAL_FLAG_DESTROY);
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

	void wl_output_set_user_data(struct wl_output *wl_output, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy*) wl_output, user_data);
	}

	void* wl_output_get_user_data(struct wl_output *wl_output) {
		return wl_proxy_get_user_data((struct wl_proxy*) wl_output);
	}

	void wl_output_destroy(struct wl_output *wl_output) {
		wl_proxy_destroy((struct wl_proxy*) wl_output);
	}

	int wl_seat_add_listener(struct wl_seat *wl_seat, const struct wl_seat_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_seat, (void (**)(void)) listener, data);
	}

	void wl_seat_set_user_data(struct wl_seat *wl_seat, void *user_data) {
		wl_proxy_set_user_data((struct wl_proxy*) wl_seat, user_data);
	}

	void* wl_seat_get_user_data(struct wl_seat *wl_seat) {
		return wl_proxy_get_user_data((struct wl_proxy*) wl_seat);
	}

	struct wl_pointer *wl_seat_get_pointer(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_seat, WL_SEAT_GET_POINTER, wl_pointer_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_seat), 0, NULL);

		return (struct wl_pointer*) id;
	}

	struct wl_keyboard *wl_seat_get_keyboard(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_seat, WL_SEAT_GET_KEYBOARD, wl_keyboard_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_seat), 0, NULL);

		return (struct wl_keyboard*) id;
	}

	struct wl_touch *wl_seat_get_touch(struct wl_seat *wl_seat) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wl_seat, WL_SEAT_GET_TOUCH, wl_touch_interface,
				wl_proxy_get_version((struct wl_proxy*) wl_seat), 0, NULL);

		return (struct wl_touch*) id;
	}

	void wl_seat_destroy(struct wl_seat *wl_seat) {
		wl_proxy_destroy((struct wl_proxy*) wl_seat);
	}

	int wl_pointer_add_listener(struct wl_pointer *wl_pointer, const struct wl_pointer_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_pointer, (void (**)(void)) listener, data);
	}

	void wl_pointer_set_cursor(struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, int32_t hotspot_x, int32_t hotspot_y) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_pointer, WL_POINTER_SET_CURSOR, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_pointer), 0, serial, surface, hotspot_x, hotspot_y);
	}

	void wl_pointer_release(struct wl_pointer *wl_pointer) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_pointer, WL_POINTER_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_pointer), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_keyboard_add_listener(struct wl_keyboard *wl_keyboard, const struct wl_keyboard_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_keyboard, (void (**)(void)) listener, data);
	}

	void wl_keyboard_release(struct wl_keyboard *wl_keyboard) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_keyboard, WL_KEYBOARD_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_keyboard), WL_MARSHAL_FLAG_DESTROY);
	}

	int wl_touch_add_listener(struct wl_touch *wl_touch, const struct wl_touch_listener *listener, void *data) {
		return wl_proxy_add_listener((struct wl_proxy*) wl_touch, (void (**)(void)) listener, data);
	}

	void wl_touch_release(struct wl_touch *wl_touch) {
		wl_proxy_marshal_flags((struct wl_proxy*) wl_touch, WL_TOUCH_RELEASE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wl_touch), WL_MARSHAL_FLAG_DESTROY);
	}

	struct wp_viewport* wp_viewporter_get_viewport(struct wp_viewporter *wp_viewporter, struct wl_surface *surface) {
		struct wl_proxy *id;

		id = wl_proxy_marshal_flags((struct wl_proxy*) wp_viewporter, WP_VIEWPORTER_GET_VIEWPORT, wp_viewport_interface,
				wl_proxy_get_version((struct wl_proxy*) wp_viewporter), 0, NULL, surface);

		return (struct wp_viewport*) id;
	}

	void wp_viewporter_destroy(struct wp_viewporter *wp_viewporter) {
		wl_proxy_marshal_flags((struct wl_proxy*) wp_viewporter, WP_VIEWPORTER_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wp_viewporter), WL_MARSHAL_FLAG_DESTROY);
	}

	void wp_viewport_set_source(struct wp_viewport *wp_viewport, wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wp_viewport, WP_VIEWPORT_SET_SOURCE, NULL,
				wl_proxy_get_version((struct wl_proxy*) wp_viewport), 0, x, y, width, height);
	}

	void wp_viewport_set_destination(struct wp_viewport *wp_viewport, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) wp_viewport, WP_VIEWPORT_SET_DESTINATION, NULL,
				wl_proxy_get_version((struct wl_proxy*) wp_viewport), 0, width, height);
	}

	void wp_viewport_destroy(struct wp_viewport *wp_viewport) {
		wl_proxy_marshal_flags((struct wl_proxy*) wp_viewport, WP_VIEWPORT_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) wp_viewport), WL_MARSHAL_FLAG_DESTROY);
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

	void xdg_surface_set_window_geometry(struct xdg_surface *xdg_surface, int32_t x, int32_t y, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_surface,
		XDG_SURFACE_SET_WINDOW_GEOMETRY, NULL, wl_proxy_get_version((struct wl_proxy*) xdg_surface), 0, x, y, width, height);
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

	void xdg_toplevel_move(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_MOVE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, seat, serial);
	}

	void xdg_toplevel_resize(struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, uint32_t edges) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_RESIZE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, seat, serial, edges);
	}

	void xdg_toplevel_set_max_size(struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_MAX_SIZE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, width, height);
	}

	void xdg_toplevel_set_min_size(struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_MIN_SIZE, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, width, height);
	}

	void xdg_toplevel_set_maximized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_MAXIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0);
	}

	void xdg_toplevel_unset_maximized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_UNSET_MAXIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0);
	}

	void xdg_toplevel_set_fullscreen(struct xdg_toplevel *xdg_toplevel, struct wl_output *output) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0, output);
	}

	void xdg_toplevel_unset_fullscreen(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0);
	}

	void xdg_toplevel_set_minimized(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_SET_MINIMIZED, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), 0);
	}

	void xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_toplevel), WL_MARSHAL_FLAG_DESTROY);
	}

	void xdg_wm_base_destroy(struct xdg_wm_base *xdg_wm_base) {
		wl_proxy_marshal_flags((struct wl_proxy*) xdg_wm_base, XDG_WM_BASE_DESTROY, NULL,
				wl_proxy_get_version((struct wl_proxy*) xdg_wm_base), WL_MARSHAL_FLAG_DESTROY);
	}

	wl_cursor_theme * (* wl_cursor_theme_load) (const char *name, int size, struct wl_shm *shm) = nullptr;
	void (* wl_cursor_theme_destroy) (wl_cursor_theme *theme) = nullptr;
	wl_cursor * (* wl_cursor_theme_get_cursor) (wl_cursor_theme *theme, const char *name) = nullptr;
	wl_buffer * (* wl_cursor_image_get_buffer) (wl_cursor_image *image) = nullptr;

	ConnectionData acquireConnection();
	ConnectionData getActiveConnection() const;

	bool hasWaylandCursor() const { return _cursor != nullptr; }

protected:
	bool open(void *);
	bool openWaylandCursor(void *d);

	void openConnection(ConnectionData &data);

	void *_handle = nullptr;
	void *_cursor = nullptr;

	ConnectionData _pending;
	ConnectionData _current;
};

struct WaylandDisplay : Ref {
	virtual ~WaylandDisplay();

	bool init(const Rc<WaylandLibrary> &);

	wl_surface *createSurface(WaylandView *);
	void destroySurface(wl_surface *);

	wl_surface *createDecorationSurface(WaylandDecoration *);
	void destroyDecorationSurface(wl_surface *);

	bool ownsSurface(wl_surface *) const;
	bool isDecoration(wl_surface *) const;

	bool flush();

	int getSocketFd() const;

	Rc<WaylandLibrary> wayland;
	wl_display *display = nullptr;

	Rc<WaylandShm> shm;
	Rc<WaylandSeat> seat;
	Rc<XkbLibrary> xkb;
	Vector<Rc<WaylandOutput>> outputs;
	wl_compositor *compositor = nullptr;
	wl_subcompositor *subcompositor = nullptr;
	wp_viewporter *viewporter = nullptr;
	xdg_wm_base *xdgWmBase = nullptr;
	Set<wl_surface *> surfaces;
	Set<wl_surface *> decorations;
	bool seatDirty = false;
};

enum class WaylandCursorImage {
	LeftPtr,
	EResize,
	NEResize,
	NResize,
	NWResize,
	SEResize,
	SResize,
	SWResize,
	WResize,

	RightSide = EResize,
	TopRigntCorner = NEResize,
	TopSide = NResize,
	TopLeftCorner = NWResize,
	BottomRightCorner = SEResize,
	BottomSide = SResize,
	BottomLeftCorner = SWResize,
	LeftSide = WResize,

	Max
};

enum class WaylandDecorationName {
	RightSide,
	TopRigntCorner,
	TopSide,
	TopLeftCorner,
	BottomRightCorner,
	BottomSide,
	BottomLeftCorner,
	LeftSide,
	HeaderLeft,
	HeaderRight,
	HeaderCenter,
	HeaderBottom,
	IconClose,
	IconMaximize,
	IconMinimize,
	IconRestore
};

struct WaylandCursorTheme : public Ref {
	~WaylandCursorTheme();

	bool init(WaylandDisplay *, const char *name, int size);

	void setCursor(WaylandSeat *);
	void setCursor(wl_pointer *, wl_surface *, uint32_t serial, WaylandCursorImage img, int scale);

	Rc<WaylandLibrary> wayland;
	wl_cursor_theme *cursorTheme = nullptr;
	int cursorSize = 24;
	String cursorName;
	Vector<wl_cursor *> cursors;
};

struct WaylandBuffer : Ref {
	virtual ~WaylandBuffer();

	bool init(WaylandLibrary *lib, wl_shm_pool *wl_shm_pool, int32_t offset,
			int32_t width, int32_t height, int32_t stride, uint32_t format);

	Rc<WaylandLibrary> wayland;
	wl_buffer *buffer = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct WaylandShm : Ref {
	enum Format {
		ARGB = WL_SHM_FORMAT_ARGB8888,
		xRGB = WL_SHM_FORMAT_XRGB8888
	};

	struct ShadowBuffers {
		Rc<WaylandBuffer> top;
		Rc<WaylandBuffer> left;
		Rc<WaylandBuffer> bottom;
		Rc<WaylandBuffer> right;
		Rc<WaylandBuffer> topLeft;
		Rc<WaylandBuffer> topRight;
		Rc<WaylandBuffer> bottomLeft;
		Rc<WaylandBuffer> bottomRight;

		Rc<WaylandBuffer> topActive;
		Rc<WaylandBuffer> leftActive;
		Rc<WaylandBuffer> bottomActive;
		Rc<WaylandBuffer> rightActive;
		Rc<WaylandBuffer> topLeftActive;
		Rc<WaylandBuffer> topRightActive;
		Rc<WaylandBuffer> bottomLeftActive;
		Rc<WaylandBuffer> bottomRightActive;

		Rc<WaylandBuffer> headerLeft;
		Rc<WaylandBuffer> headerLeftActive;
		Rc<WaylandBuffer> headerRight;
		Rc<WaylandBuffer> headerRightActive;
		Rc<WaylandBuffer> headerCenter;
		Rc<WaylandBuffer> headerCenterActive;

		Rc<WaylandBuffer> iconClose;
		Rc<WaylandBuffer> iconMaximize;
		Rc<WaylandBuffer> iconMinimize;
		Rc<WaylandBuffer> iconRestore;

		Rc<WaylandBuffer> iconCloseActive;
		Rc<WaylandBuffer> iconMaximizeActive;
		Rc<WaylandBuffer> iconMinimizeActive;
		Rc<WaylandBuffer> iconRestoreActive;
	};

	virtual ~WaylandShm();

	bool init(const Rc<WaylandLibrary> &, wl_registry *, uint32_t name, uint32_t version);

	bool allocateDecorations(ShadowBuffers *ret, uint32_t width, uint32_t inset,
			const Color3B &header, const Color3B &headerActive);

	Rc<WaylandLibrary> wayland;
	uint32_t id;
	wl_shm *shm;
	uint32_t format;
};

struct WaylandOutput : Ref {
	struct Geometry {
		int32_t x;
		int32_t y;
		int32_t physical_width;
		int32_t physical_height;
		int32_t subpixel;
		int32_t transform;
		String make;
		String model;
	};

	struct Mode {
		uint32_t flags;
		int32_t width;
		int32_t height;
		int32_t refresh;
	};

	virtual ~WaylandOutput();

	bool init(const Rc<WaylandLibrary> &, wl_registry *, uint32_t name, uint32_t version);

	String description() const;

	Rc<WaylandLibrary> wayland;
	uint32_t id;
	wl_output *output;
	Geometry geometry;
	Mode mode;
	int32_t scale;

	String name;
	String desc;
};

struct WaylandSeat : Ref {
	struct KeyState {
		xkb_mod_index_t controlIndex = 0;
		xkb_mod_index_t altIndex = 0;
		xkb_mod_index_t shiftIndex = 0;
		xkb_mod_index_t superIndex = 0;
		xkb_mod_index_t capsLockIndex = 0;
		xkb_mod_index_t numLockIndex = 0;

		int32_t keyRepeatRate = 0;
		int32_t keyRepeatDelay = 0;
		int32_t keyRepeatInterval = 0;
		uint32_t modsDepressed = 0;
		uint32_t modsLatched = 0;
		uint32_t modsLocked = 0;

		InputKeyCode keycodes[256] = { InputKeyCode::Unknown };
	};

	virtual ~WaylandSeat();

	bool init(const Rc<WaylandLibrary> &, WaylandDisplay *, wl_registry *, uint32_t name, uint32_t version);
	void initCursors();
	void tryUpdateCursor();

	void update();

	InputKeyCode translateKey(uint32_t scancode) const;
	xkb_keysym_t composeSymbol(xkb_keysym_t sym) const;

	Rc<WaylandLibrary> wayland;
	WaylandDisplay *root;
	uint32_t id;
	bool hasPointerFrames = false;
	wl_seat *seat;
	String name;
	uint32_t capabilities;
	wl_pointer *pointer;
	wl_keyboard *keyboard;
	wl_touch *touch;
	int32_t pointerScale = 1;
	wl_surface *pointerFocus = nullptr;
	uint32_t serial = 0;
	wl_surface *cursorSurface = nullptr;
	xkb_state *state = nullptr;
	xkb_compose_state *compose = nullptr;
	KeyState keyState;

	Rc<WaylandCursorTheme> cursorTheme;
	WaylandCursorImage cursorImage = WaylandCursorImage::Max;

	Set<WaylandDecoration *> pointerDecorations;
	Set<WaylandOutput *> pointerOutputs;
	Set<WaylandView *> pointerViews;
	Set<WaylandView *> keyboardViews;
};

struct WaylandDecoration : Ref {
	virtual ~WaylandDecoration();

	bool init(WaylandView *, Rc<WaylandBuffer> &&buffer, Rc<WaylandBuffer> &&active, WaylandDecorationName);
	bool init(WaylandView *, Rc<WaylandBuffer> &&buffer, WaylandDecorationName);

	void setAltBuffers(Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a);
	void handlePress(uint32_t serial, uint32_t button, uint32_t state);
	void handleMotion();

	void onEnter();
	void onLeave();

	void setActive(bool);
	void setVisible(bool);
	void setAlternative(bool);
	void setGeometry(int32_t x, int32_t y, int32_t width, int32_t height);

	bool commit();

	bool isTouchable() const;

	WaylandLibrary *wayland = nullptr;
	WaylandDisplay *display = nullptr;;
	WaylandView *root = nullptr;
	WaylandDecorationName name = WaylandDecorationName::HeaderCenter;
	WaylandCursorImage image = WaylandCursorImage::LeftPtr;
	wl_surface *surface = nullptr;
	wl_subsurface *subsurface = nullptr;
	wp_viewport *viewport = nullptr;
	Rc<WaylandBuffer> buffer;
	Rc<WaylandBuffer> active;

	Rc<WaylandBuffer> altBuffer;
	Rc<WaylandBuffer> altActive;

	int32_t _x = 0;
	int32_t _y = 0;
	int32_t _width = 0;
	int32_t _height = 0;
	uint64_t lastTouch = 0;
	uint32_t serial = 0;

	bool visible = true;
	bool isActive = false;
	bool alternative = false;
	bool dirty = false;
	bool waitForMove = false;
};

class WaylandView : public LinuxViewInterface {
public:
	static constexpr auto DecorWidth = 20;
	static constexpr auto DecorInset = 16;
	static constexpr auto DecorOffset = 6;
	static constexpr auto IconSize = DecorInset + DecorOffset;

	struct PointerEvent {
		enum Event : uint32_t {
			None,
			Enter,
			Leave,
			Motion,
			Button,
			Axis,
			AxisSource,
			AxisStop,
			AxisDiscrete
		};

		Event event;
		union {
			struct {
				wl_fixed_t x;
				wl_fixed_t y;
			} enter;
			struct {
				uint32_t time;
				wl_fixed_t x;
				wl_fixed_t y;
			} motion;
			struct {
				uint32_t serial;
				uint32_t time;
				uint32_t button;
				uint32_t state;
			} button;
			struct {
				uint32_t time;
				uint32_t axis;
				wl_fixed_t value;
			} axis;
			struct {
				uint32_t axis_source;
			} axisSource;
			struct {
				uint32_t time;
				uint32_t axis;
			} axisStop;
			struct {
				uint32_t axis;
				int32_t discrete;
			} axisDiscrete;
		};
	};

	WaylandView(WaylandLibrary *, ViewImpl *, StringView, URect);
	virtual ~WaylandView();

	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const override;

	virtual bool poll(bool frameReady) override;

	virtual int getSocketFd() const override;

	virtual uint64_t getScreenFrameInterval() const override;

	virtual void mapWindow() override;

	WaylandDisplay *getDisplay() const { return _display; }
	wl_surface *getSurface() const { return _surface; }

	void handleSurfaceEnter(wl_surface *surface, wl_output *output);
	void handleSurfaceLeave(wl_surface *surface, wl_output *output);

	void handleSurfaceConfigure(xdg_surface *, uint32_t serial);
	void handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states);
	void handleToplevelClose(xdg_toplevel *xdg_toplevel);
	void handleToplevelBounds(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
	void handleSurfaceFrameDone(wl_callback *wl_callback, uint32_t callback_data);

	void handlePointerEnter(wl_fixed_t surface_x, wl_fixed_t surface_y);
	void handlePointerLeave();
	void handlePointerMotion(uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
	void handlePointerButton(uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	void handlePointerAxis(uint32_t time, uint32_t axis, wl_fixed_t value);
	void handlePointerAxisSource(uint32_t axis_source);
	void handlePointerAxisStop(uint32_t time, uint32_t axis);
	void handlePointerAxisDiscrete(uint32_t axis, int32_t discrete);
	void handlePointerFrame();

	void handleKeyboardEnter(Vector<uint32_t> &&, uint32_t depressed, uint32_t latched, uint32_t locked);
	void handleKeyboardLeave();
	void handleKey(uint32_t time, uint32_t key, uint32_t state);
	void handleKeyModifiers(uint32_t depressed, uint32_t latched, uint32_t locked);
	void handleKeyRepeat();

	void handleDecorationPress(WaylandDecoration *, uint32_t serial, bool released = false);

	virtual void scheduleFrame() override;

	virtual void onSurfaceInfo(gl::SurfaceInfo &) const override;

	virtual void commit(uint32_t width, uint32_t height) override;

protected:
	void createDecorations();

	Rc<WaylandDisplay> _display;
	ViewImpl *_view = nullptr;

	wl_surface *_surface = nullptr;
	xdg_surface *_xdgSurface = nullptr;
	xdg_toplevel *_toplevel = nullptr;
	Extent2 _currentExtent;
	Extent2 _commitedExtent;

	bool _continuousRendering = true;
	bool _scheduleNext = false;
	bool _clientSizeDecoration = true;
	bool _shouldClose = false;
	bool _surfaceDirty = false;
	bool _fullscreen = false;
	bool _pointerInit = false;

	Set<WaylandOutput *> _activeOutputs;

	double _surfaceX = 0.0;
	double _surfaceY = 0.0;
	InputModifier _activeModifiers = InputModifier::None;
	Vector<PointerEvent> _pointerEvents;

	std::bitset<size_t(XDG_TOPLEVEL_STATE_TILED_BOTTOM + 1)> _state;
	Vector<Rc<WaylandDecoration>> _decors;
	Rc<WaylandDecoration> _iconMaximized;

	uint32_t _configureSerial = maxOf<uint32_t>();
	uint64_t _screenFrameInterval = 0;

	struct KeyData {
		uint32_t scancode;
		char32_t codepoint;
		uint64_t time;
		bool repeats;
		uint64_t lastRepeat = 0;
	};

	Map<uint32_t, KeyData> _keys;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXWAYLAND_H_ */
