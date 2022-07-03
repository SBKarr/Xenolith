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

namespace stappler::xenolith::platform {

static void WaylandView_registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	((WaylandView *)data)->handleGlobal(registry, name, interface, version);
}

static void WaylandView_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	((WaylandView *)data)->handleGlobalRemove(registry, name);
}

static void WaylandView_handle_output_geometry(void *data, wl_output *wl_output, int32_t x, int32_t y,
		int32_t physical_width, int32_t physical_height, int32_t subpixel, char const *make, char const *model, int32_t transform) {
	((WaylandView *)data)->handleOutputGeometry(wl_output, x, y, physical_width, physical_height, subpixel, make, model, transform);
}

static void WaylandView_handle_output_mode(void *data, wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
	((WaylandView *)data)->handleOutputMode(output, flags, width, height, refresh);
}

static void WaylandView_handle_output_done(void *data, wl_output *output) {
	((WaylandView *)data)->handleOutputDone(output);
}

static void WaylandView_handle_output_scale(void *data, wl_output* output, int32_t factor) {
	((WaylandView *)data)->handleOutputScale(output, factor);
}

static void WaylandView_handle_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
	((WaylandView *)data)->getWayland()->xdg_wm_base_pong(xdg_wm_base, serial);
}

static void WaylandView_handle_xdg_surface_configure(void *data, xdg_surface *xdg_surface, uint32_t serial) {
	((WaylandView *)data)->handleSurfaceConfigure(xdg_surface, serial);
}

static void WaylandView_handle_xdg_toplevel_configure(void *data, xdg_toplevel *xdg_toplevel,
		int32_t width, int32_t height, wl_array *states) {
	((WaylandView *)data)->handleToplevelConfigure(xdg_toplevel, width, height, states);
}

static void WaylandView_handle_xdg_toplevel_close(void* data, struct xdg_toplevel* xdg_toplevel) {
	((WaylandView *)data)->handleToplevelClose(xdg_toplevel);
}

static void WaylandView_handle_surface_frame_done(void *data, wl_callback *wl_callback, uint32_t callback_data) {
	((WaylandView *)data)->handleSurfaceFrameDone(wl_callback, callback_data);
}

static const wl_registry_listener s_WaylandRegistryListener{
	WaylandView_registry_handle_global,
	WaylandView_registry_handle_global_remove,
};

static const wl_output_listener s_WaylandOutputListener{
	WaylandView_handle_output_geometry,
	WaylandView_handle_output_mode,
	WaylandView_handle_output_done,
	WaylandView_handle_output_scale
};

static const xdg_wm_base_listener s_XdgWmBaseListener{
	WaylandView_handle_xdg_wm_base_ping,
};

static xdg_surface_listener const s_XdgSurfaceListener{
	WaylandView_handle_xdg_surface_configure,
};

static const xdg_toplevel_listener s_XdgToplevelListener{
	WaylandView_handle_xdg_toplevel_configure,
	WaylandView_handle_xdg_toplevel_close,
};

static const wl_callback_listener s_WaylandSurfaceFrameListener{
	WaylandView_handle_surface_frame_done,
};

WaylandView::WaylandView(WaylandLibrary *lib, ViewImpl *view, StringView name, URect rect) {
	_wayland = lib;
	_view = view;
	_currentExtent = Extent2(rect.width, rect.height);

	auto connection = _wayland->acquireConnection();

	_display = connection.display;

	struct wl_registry *registry = _wayland->wl_display_get_registry(_display);
	_wayland->wl_registry_add_listener(registry, &s_WaylandRegistryListener, this);
	_wayland->wl_display_roundtrip(_display);
	_wayland->wl_registry_destroy(registry);

	if (_compositor && _xdgWmBase) {
		_surface = _wayland->wl_compositor_create_surface(_compositor);
		_xdgSurface = _wayland->xdg_wm_base_get_xdg_surface(_xdgWmBase, _surface);

		_wayland->xdg_surface_add_listener(_xdgSurface, &s_XdgSurfaceListener, this);
		_toplevel = _wayland->xdg_surface_get_toplevel(_xdgSurface);
		_wayland->xdg_toplevel_set_title(_toplevel, name.data());
		_wayland->xdg_toplevel_set_app_id(_toplevel, view->getLoop()->getApplication()->getData().bundleName.data());
		_wayland->xdg_toplevel_add_listener(_toplevel, &s_XdgToplevelListener, this);

		auto region = _wayland->wl_compositor_create_region(_compositor);
		_wayland->wl_region_add(region, 0, 0, _currentExtent.width, _currentExtent.height);
		_wayland->wl_surface_set_opaque_region(_surface, region);
		_wayland->wl_surface_commit(_surface);
		_wayland->wl_region_destroy(region);
	}
}

WaylandView::~WaylandView() {
	if (_toplevel) {
		_wayland->xdg_toplevel_destroy(_toplevel);
		_toplevel = nullptr;
	}
	if (_surface) {
		_wayland->wl_surface_destroy(_surface);
		_surface = nullptr;
	}
	if (_xdgSurface) {
		_wayland->xdg_surface_destroy(_xdgSurface);
		_xdgSurface = nullptr;
	}
	if (_output) {
		_wayland->wl_output_destroy(_output);
		_output = nullptr;
	}
	if (_xdgWmBase) {
		_wayland->xdg_wm_base_destroy(_xdgWmBase);
		_xdgWmBase = nullptr;
	}
	if (_compositor) {
		_wayland->wl_compositor_destroy(_compositor);
		_compositor = nullptr;
	}
	if (_display) {
		_wayland->wl_display_disconnect(_display);
		_display = nullptr;
	}
}

VkSurfaceKHR WaylandView::createWindowSurface(vk::Instance *instance) const {
	VkSurfaceKHR ret;
	VkWaylandSurfaceCreateInfoKHR info{
		VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		nullptr,
		0,
		_display,
		_surface
	};

	if (instance->vkCreateWaylandSurfaceKHR(instance->getInstance(), &info, nullptr, &ret) == VK_SUCCESS) {
		return ret;
	}
	return nullptr;
}

bool WaylandView::poll() {
	auto frame = _wayland->wl_surface_frame(_surface);
	_wayland->wl_callback_add_listener(frame, &s_WaylandSurfaceFrameListener, this);
	_wayland->wl_surface_commit(_surface);

	flush();

	return true;
}

int WaylandView::getSocketFd() const {
	return _wayland->wl_display_get_fd(_display);
}

uint64_t WaylandView::getScreenFrameInterval() const {
	return 0; // 16'000;
}

void WaylandView::mapWindow() {
	flush();
}

void WaylandView::handleGlobal(struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	printf("handleGlobal: '%s', version: %d, name: %d\n", interface, version, name);
	if (strcmp(interface, _wayland->wl_compositor_interface->name) == 0) {
		_compositor = static_cast<wl_compositor *>(_wayland->wl_registry_bind(registry, name,
				_wayland->wl_compositor_interface, std::min(version, 4U)));
	} else if (strcmp(interface, _wayland->wl_output_interface->name) == 0) {
		if (!_output) {
			_output = static_cast<wl_output*>(_wayland->wl_registry_bind(registry, name,
					_wayland->wl_output_interface, std::min(version, 2U)));

			_wayland->wl_output_add_listener(_output, &s_WaylandOutputListener, this);
			_wayland->wl_display_roundtrip(_display);
		}
	} else if (strcmp(interface, _wayland->xdg_wm_base_interface->name) == 0) {
		_xdgWmBase = static_cast<struct xdg_wm_base *>(_wayland->wl_registry_bind(registry, name,
				_wayland->xdg_wm_base_interface, std::min(version, 2U)));
		_wayland->xdg_wm_base_add_listener(_xdgWmBase, &s_XdgWmBaseListener, this);
	}
}

void WaylandView::handleGlobalRemove(struct wl_registry *registry, uint32_t name) {
	printf("handleGlobalRemove: name: %d\n", name);
}

void WaylandView::handleOutputGeometry(wl_output *output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, char const *make, char const *model, int32_t transform) {
	printf("handleOutputMode: x: %d, y: %d, width: %d, height: %d, subpixel: %d, make: '%s', model: '%s', transform: %d\n",
			x, y, physical_width, physical_height, subpixel, make, model, transform);
}

void WaylandView::handleOutputMode(wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	printf("handleOutputMode: flags: %d, width: %d, height: %d, refresh: %d\n", flags, width, height, refresh);
}

void WaylandView::handleOutputDone(wl_output *output) {
	printf("handleOutputDone\n");
}

void WaylandView::handleOutputScale(wl_output *output, int32_t factor) {
	printf("handleOutputScale: factor: %d\n", factor);
}

void WaylandView::handleSurfaceConfigure(xdg_surface *surface, uint32_t serial) {
	printf("handleSurfaceConfigure: serial: %d\n", serial);
	_wayland->xdg_surface_ack_configure(surface, serial);
}

void WaylandView::handleToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states) {
	printf("handleToplevelConfigure: width: %d, height: %d\n", width, height);
}

void WaylandView::handleToplevelClose(xdg_toplevel *xdg_toplevel) {
	printf("handleToplevelClose\n");
}

void WaylandView::handleSurfaceFrameDone(wl_callback *callback, uint32_t data) {
	printf("handleSurfaceFrameDone: data: %d\n", data);
	_wayland->wl_callback_destroy(callback);
}

void WaylandView::onSurfaceInfo(gl::SurfaceInfo &info) const {
	info.currentExtent = _currentExtent;
}

bool WaylandView::flush() {
	while (_wayland->wl_display_prepare_read(_display) != 0) {
		_wayland->wl_display_dispatch_pending(_display);
	}

	_wayland->wl_display_read_events(_display);
	_wayland->wl_display_dispatch_pending(_display);

	while (_wayland->wl_display_flush(_display) == -1) {
		if (errno != EAGAIN) {
			return false;
		}

		struct pollfd fd = { getSocketFd(), POLLOUT };

		while (::poll(&fd, 1, -1) == -1) {
			if (errno != EINTR && errno != EAGAIN) {
				return false;
			}
		}
	}

	return true;
}

}

#endif
