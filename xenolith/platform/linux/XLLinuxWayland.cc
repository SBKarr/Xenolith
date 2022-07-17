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
#include <sys/mman.h>

namespace stappler::xenolith::platform {

static const char *s_XenolithWaylandTag = "org.stappler.xenolith.wayland";

static WaylandLibrary *s_WaylandLibrary = nullptr;

struct ViewporterInterface {
	const struct wl_interface *viewporter_types[6];

	const struct wl_message wp_viewporter_requests[2];
	const struct wl_message wp_viewport_requests[3];

	const struct wl_interface wp_viewporter_interface;
	const struct wl_interface wp_viewport_interface;

	ViewporterInterface(const struct wl_interface *wl_surface_interface)
	: viewporter_types{
		NULL,
		NULL,
		NULL,
		NULL,
		&wp_viewport_interface,
		wl_surface_interface
	}, wp_viewporter_requests{
		{ "destroy", "", viewporter_types + 0 },
		{ "get_viewport", "no", viewporter_types + 4 },
	}, wp_viewport_requests{
		{ "destroy", "", viewporter_types + 0 },
		{ "set_source", "ffff", viewporter_types + 0 },
		{ "set_destination", "ii", viewporter_types + 0 },
	}, wp_viewporter_interface{
		"wp_viewporter", 1,
		2, wp_viewporter_requests,
		0, NULL,
	}, wp_viewport_interface{
		"wp_viewport", 1,
		3, wp_viewport_requests,
		0, NULL,
	} { }
};

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

static const uint8_t s_iconClose[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0xff, 0xbd, 0x8f, 0x67, 0x67, 0x67, 0x5c, 0x55, 0x55, 0x55,
	0x6, 0x20, 0x0, 0x5, 0x0, 0x1c, 0x0, 0x0, 0x24, 0x0, 0xf, 0x54, 0x0, 0x1d, 0x8e, 0x67,
	0x67, 0x67, 0xff, 0x67, 0x67, 0x67, 0xb0, 0x5c, 0x0, 0x5, 0x54, 0x0, 0x13, 0xb0, 0x24, 0x0,
	0xf, 0x5c, 0x0, 0x16, 0x83, 0x66, 0x66, 0x66, 0x5, 0x67, 0x67, 0x67, 0xaf, 0x38, 0x0, 0x5b,
	0xb5, 0x6d, 0x6d, 0x6d, 0x7, 0x54, 0x0, 0xe, 0x1c, 0x0, 0xf, 0x5c, 0x0, 0x20, 0x40, 0x68,
	0x68, 0x68, 0xb6, 0x40, 0x0, 0xf, 0x54, 0x0, 0x2d, 0xf, 0xf0, 0x0, 0x4, 0x18, 0xb0, 0xc,
	0x0, 0xf, 0x7c, 0x1, 0x9, 0xf, 0x14, 0x1, 0x20, 0xf, 0xa8, 0x0, 0x2a, 0xe, 0xb4, 0x0,
	0x6, 0x58, 0x0, 0x4, 0x14, 0x1, 0xf, 0x54, 0x0, 0x30, 0x1, 0xfc, 0x0, 0x4f, 0x66, 0x66,
	0x66, 0xb1, 0x8, 0x1, 0x31, 0xf, 0x14, 0x2, 0x1, 0xf, 0xcc, 0x1, 0x1, 0xf, 0x54, 0x0,
	0x2d, 0xf, 0x1c, 0x0, 0x11, 0xf, 0x18, 0x3, 0x15, 0x0, 0xfc, 0x0, 0xf, 0x18, 0x3, 0x39,
	0xf, 0xc8, 0x3, 0x45, 0xf, 0x1, 0x0, 0xff, 0x92, 0xf0, 0x0, 0x66, 0x68, 0x65, 0x69, 0x67,
	0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconCloseActive[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0x4, 0xf3, 0x5, 0xb8, 0xb8, 0xb8, 0x12, 0xb4, 0xb4, 0xb4,
	0x62, 0xb4, 0xb4, 0xb4, 0xb0, 0xb3, 0xb3, 0xb3, 0xd8, 0xb3, 0xb3, 0xb3, 0xf4, 0x4, 0x0, 0x0,
	0xc, 0x0, 0xaf, 0xaf, 0xb3, 0xb3, 0xb3, 0x61, 0xb4, 0xb4, 0xb4, 0x11, 0x0, 0x1, 0x0, 0x14,
	0xff, 0x1, 0xaa, 0xaa, 0xaa, 0x9, 0xb2, 0xb2, 0xb2, 0x7b, 0xb3, 0xb3, 0xb3, 0xf1, 0xb3, 0xb3,
	0xb3, 0xff, 0x4, 0x0, 0xc, 0x50, 0xf0, 0xb4, 0xb4, 0xb4, 0x7a, 0x34, 0x0, 0xf, 0x54, 0x0,
	0x9, 0x8f, 0xb1, 0xb1, 0xb1, 0x1a, 0xb3, 0xb3, 0xb3, 0xca, 0x50, 0x0, 0x10, 0xc, 0x4, 0x0,
	0x5f, 0xcf, 0xb3, 0xb3, 0xb3, 0x1e, 0x54, 0x0, 0x1, 0x8f, 0xb6, 0xb6, 0xb6, 0x1c, 0xb3, 0xb3,
	0xb3, 0xe4, 0x54, 0x0, 0x20, 0x4, 0x4, 0x0, 0x5c, 0xe3, 0xb3, 0xb3, 0xb3, 0x1b, 0xfc, 0x0,
	0x4f, 0xb3, 0xb3, 0xb3, 0xcd, 0x54, 0x0, 0x28, 0x1, 0x4, 0x0, 0x84, 0xb2, 0xb2, 0xb2, 0xcb,
	0xbf, 0xbf, 0xbf, 0x8, 0x58, 0x0, 0x4c, 0xb4, 0xb4, 0xb4, 0x7d, 0x24, 0x0, 0x7f, 0xce, 0xce,
	0xce, 0xff, 0xb5, 0xb5, 0xb5, 0x44, 0x0, 0x6, 0x0, 0x1c, 0x0, 0x0, 0x24, 0x0, 0x1e, 0xb3,
	0x1c, 0x0, 0x14, 0x78, 0xf8, 0x1, 0xc, 0xa4, 0x1, 0x0, 0x30, 0x0, 0x0, 0x1, 0x0, 0x3e,
	0xe7, 0xe7, 0xe7, 0x5c, 0x0, 0x3, 0x54, 0x0, 0x0, 0x1c, 0x0, 0x0, 0x1, 0x0, 0xe, 0x5c,
	0x0, 0x1, 0xcc, 0x1, 0x5c, 0x11, 0xb2, 0xb2, 0xb2, 0x63, 0x38, 0x0, 0x35, 0xb4, 0xb4, 0xb4,
	0x38, 0x0, 0x39, 0xe9, 0xe9, 0xe9, 0x5c, 0x0, 0x8, 0x54, 0x0, 0xc, 0x1c, 0x0, 0x7, 0x10,
	0x1, 0x13, 0x60, 0xa0, 0x2, 0x1f, 0xff, 0x5c, 0x0, 0x11, 0xf, 0x54, 0x0, 0x11, 0x3, 0x4,
	0x0, 0x5e, 0xae, 0xb3, 0xb3, 0xb3, 0xdb, 0x44, 0x1, 0xe, 0x9c, 0x0, 0x0, 0x8, 0x0, 0x8,
	0xc, 0x0, 0xf, 0x7c, 0x1, 0x9, 0x3, 0x3c, 0x3, 0x1f, 0xf3, 0xe4, 0x1, 0x9, 0x8, 0xb8,
	0x0, 0xf, 0xa8, 0x0, 0x10, 0x4, 0x4, 0x0, 0x1e, 0xf3, 0x58, 0x0, 0xe, 0xb4, 0x0, 0xf,
	0x58, 0x0, 0x20, 0x0, 0x8, 0x2, 0x1f, 0xd9, 0x54, 0x0, 0x11, 0x3f, 0xe8, 0xe8, 0xe8, 0x8,
	0x1, 0x19, 0x13, 0xd7, 0x44, 0x4, 0xf, 0xb8, 0x1, 0x12, 0xf, 0x28, 0x2, 0x9, 0xb, 0x10,
	0x2, 0x10, 0xad, 0x98, 0x4, 0xf, 0x68, 0x2, 0x19, 0xf, 0x5c, 0x0, 0x14, 0x13, 0x5e, 0x18,
	0x3, 0x1e, 0xf0, 0x18, 0x3, 0x2, 0xfc, 0x0, 0xf, 0x18, 0x3, 0x20, 0x53, 0xef, 0xaf, 0xaf,
	0xaf, 0x10, 0xc8, 0x3, 0x1f, 0x7a, 0xc8, 0x3, 0x38, 0x14, 0x76, 0x20, 0x4, 0x0, 0x2c, 0x4,
	0x0, 0x34, 0x4, 0xf, 0x78, 0x4, 0x2c, 0x0, 0x4, 0x0, 0x18, 0xc9, 0x78, 0x4, 0x0, 0x1,
	0x0, 0x0, 0xe4, 0x4, 0x3, 0xec, 0x4, 0xf, 0x54, 0x0, 0x25, 0x10, 0xe2, 0xc0, 0x5, 0xf,
	0xd8, 0x5, 0x8, 0x1f, 0xc9, 0x54, 0x0, 0x20, 0x1f, 0xce, 0xd8, 0x5, 0x5, 0x8, 0x14, 0x1,
	0x0, 0xec, 0x4, 0xc, 0xcc, 0x1, 0xf, 0x4, 0x0, 0x4, 0x10, 0xef, 0x50, 0x1, 0x4f, 0xb6,
	0xb6, 0xb6, 0x7, 0xe8, 0x6, 0x15, 0x3, 0xec, 0x4, 0x13, 0x60, 0xe0, 0x2, 0x17, 0xd7, 0x9c,
	0x3, 0x10, 0xd7, 0x58, 0x4, 0x44, 0xb4, 0xb4, 0xb4, 0x5f, 0xf8, 0x1, 0xf, 0x1, 0x0, 0x1,
	0xf0, 0x0, 0x66, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68,
	0x16,
};

static const uint8_t s_iconMaximize[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0xff, 0xb9, 0x8f, 0x64, 0x64, 0x64, 0x40, 0x66, 0x66, 0x66,
	0x80, 0x4, 0x0, 0x11, 0x0, 0x2c, 0x0, 0xf, 0x58, 0x0, 0x15, 0x0, 0x30, 0x0, 0x8f, 0x65,
	0x65, 0x65, 0xfc, 0x65, 0x65, 0x65, 0xe0, 0x4, 0x0, 0xc, 0x10, 0xfc, 0x2c, 0x0, 0xf, 0x58,
	0x0, 0x1c, 0x1f, 0xe0, 0x28, 0x0, 0xd, 0x0, 0x24, 0x0, 0xf, 0x58, 0x0, 0xff, 0xff, 0x8a,
	0xf, 0x18, 0x3, 0x3e, 0xf, 0xc8, 0x3, 0x45, 0xf, 0x1, 0x0, 0xff, 0x92, 0xf0, 0x0, 0x66,
	0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconMaximizeActive[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0x4, 0xf3, 0x5, 0xb8, 0xb8, 0xb8, 0x12, 0xb4, 0xb4, 0xb4,
	0x62, 0xb4, 0xb4, 0xb4, 0xb0, 0xb3, 0xb3, 0xb3, 0xd8, 0xb3, 0xb3, 0xb3, 0xf4, 0x4, 0x0, 0x0,
	0xc, 0x0, 0xaf, 0xaf, 0xb3, 0xb3, 0xb3, 0x61, 0xb4, 0xb4, 0xb4, 0x11, 0x0, 0x1, 0x0, 0x14,
	0xff, 0x1, 0xaa, 0xaa, 0xaa, 0x9, 0xb2, 0xb2, 0xb2, 0x7b, 0xb3, 0xb3, 0xb3, 0xf1, 0xb3, 0xb3,
	0xb3, 0xff, 0x4, 0x0, 0xc, 0x50, 0xf0, 0xb4, 0xb4, 0xb4, 0x7a, 0x34, 0x0, 0xf, 0x54, 0x0,
	0x9, 0x8f, 0xb1, 0xb1, 0xb1, 0x1a, 0xb3, 0xb3, 0xb3, 0xca, 0x50, 0x0, 0x10, 0xc, 0x4, 0x0,
	0x5f, 0xcf, 0xb3, 0xb3, 0xb3, 0x1e, 0x54, 0x0, 0x1, 0x8f, 0xb6, 0xb6, 0xb6, 0x1c, 0xb3, 0xb3,
	0xb3, 0xe4, 0x54, 0x0, 0x20, 0x4, 0x4, 0x0, 0x5c, 0xe3, 0xb3, 0xb3, 0xb3, 0x1b, 0xfc, 0x0,
	0x4f, 0xb3, 0xb3, 0xb3, 0xcd, 0x54, 0x0, 0x28, 0x1, 0x4, 0x0, 0x84, 0xb2, 0xb2, 0xb2, 0xcb,
	0xbf, 0xbf, 0xbf, 0x8, 0x58, 0x0, 0x48, 0xb4, 0xb4, 0xb4, 0x7d, 0x20, 0x0, 0x7f, 0xc6, 0xc6,
	0xc6, 0xff, 0xd9, 0xd9, 0xd9, 0x4, 0x0, 0x12, 0x0, 0x2c, 0x0, 0xb, 0x60, 0x0, 0x14, 0x78,
	0xf8, 0x1, 0xc, 0xa4, 0x1, 0x0, 0x30, 0x0, 0x7f, 0xfe, 0xfe, 0xfe, 0xff, 0xf6, 0xf6, 0xf6,
	0x4, 0x0, 0xa, 0x0, 0x24, 0x0, 0x0, 0x2c, 0x0, 0x1e, 0xb3, 0xcc, 0x1, 0x5c, 0x11, 0xb2,
	0xb2, 0xb2, 0x63, 0xd0, 0x0, 0x0, 0x2c, 0x0, 0x0, 0x38, 0x0, 0xf, 0xf8, 0x0, 0xd, 0x0,
	0x24, 0x0, 0xe, 0x58, 0x0, 0x1, 0x10, 0x1, 0x13, 0x60, 0xa0, 0x2, 0xf, 0x58, 0x0, 0x39,
	0x0, 0x4, 0x0, 0x5f, 0xae, 0xb3, 0xb3, 0xb3, 0xdb, 0x58, 0x0, 0x40, 0x0, 0x3c, 0x3, 0x1f,
	0xf3, 0x58, 0x0, 0x40, 0x1f, 0xf3, 0x58, 0x0, 0x44, 0x0, 0x8, 0x2, 0x1f, 0xd9, 0x58, 0x0,
	0x40, 0x13, 0xd7, 0x44, 0x4, 0xf, 0x10, 0x2, 0x3d, 0x10, 0xad, 0x98, 0x4, 0xf, 0xb0, 0x0,
	0x40, 0x13, 0x5e, 0x18, 0x3, 0x1f, 0xf0, 0x18, 0x3, 0x38, 0x53, 0xef, 0xaf, 0xaf, 0xaf, 0x10,
	0xc8, 0x3, 0x1f, 0x7a, 0xc8, 0x3, 0x38, 0x14, 0x76, 0x20, 0x4, 0x0, 0x2c, 0x4, 0x0, 0x34,
	0x4, 0xf, 0x78, 0x4, 0x2c, 0x0, 0x4, 0x0, 0x18, 0xc9, 0x78, 0x4, 0x0, 0x1, 0x0, 0x0,
	0xe4, 0x4, 0x3, 0xec, 0x4, 0xf, 0x54, 0x0, 0x25, 0x10, 0xe2, 0xc0, 0x5, 0xf, 0xd8, 0x5,
	0x8, 0x1f, 0xc9, 0x54, 0x0, 0x20, 0x1f, 0xce, 0xd8, 0x5, 0x5, 0x8, 0x14, 0x1, 0x0, 0xec,
	0x4, 0xc, 0xcc, 0x1, 0xf, 0x4, 0x0, 0x4, 0x10, 0xef, 0x50, 0x1, 0x4f, 0xb6, 0xb6, 0xb6,
	0x7, 0xe8, 0x6, 0x15, 0x3, 0xec, 0x4, 0x13, 0x60, 0xe0, 0x2, 0x17, 0xd7, 0x9c, 0x3, 0x10,
	0xd7, 0x58, 0x4, 0x44, 0xb4, 0xb4, 0xb4, 0x5f, 0xf8, 0x1, 0xf, 0x1, 0x0, 0x1, 0xf0, 0x0,
	0x66, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconMinimize[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0xff, 0xff, 0xff, 0xff, 0x7c, 0x8f, 0x68, 0x68, 0x68, 0x20,
	0x64, 0x64, 0x64, 0x40, 0x4, 0x0, 0x11, 0x0, 0x2c, 0x0, 0xf, 0x58, 0x0, 0x15, 0x8f, 0x66,
	0x66, 0x66, 0x80, 0x65, 0x65, 0x65, 0xff, 0x4, 0x0, 0x11, 0x0, 0x2c, 0x0, 0xf, 0x58, 0x0,
	0x15, 0x40, 0x60, 0x60, 0x60, 0x10, 0x88, 0x0, 0xf, 0x4, 0x0, 0x11, 0x0, 0x2c, 0x0, 0xf,
	0x4, 0x3, 0xff, 0xff, 0x13, 0xf0, 0x0, 0x66, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16, 0x65,
	0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconMinimizeActive[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0x4, 0xf3, 0x5, 0xb8, 0xb8, 0xb8, 0x12, 0xb4, 0xb4, 0xb4,
	0x62, 0xb4, 0xb4, 0xb4, 0xb0, 0xb3, 0xb3, 0xb3, 0xd8, 0xb3, 0xb3, 0xb3, 0xf4, 0x4, 0x0, 0x0,
	0xc, 0x0, 0xaf, 0xaf, 0xb3, 0xb3, 0xb3, 0x61, 0xb4, 0xb4, 0xb4, 0x11, 0x0, 0x1, 0x0, 0x14,
	0xff, 0x1, 0xaa, 0xaa, 0xaa, 0x9, 0xb2, 0xb2, 0xb2, 0x7b, 0xb3, 0xb3, 0xb3, 0xf1, 0xb3, 0xb3,
	0xb3, 0xff, 0x4, 0x0, 0xc, 0x50, 0xf0, 0xb4, 0xb4, 0xb4, 0x7a, 0x34, 0x0, 0xf, 0x54, 0x0,
	0x9, 0x8f, 0xb1, 0xb1, 0xb1, 0x1a, 0xb3, 0xb3, 0xb3, 0xca, 0x50, 0x0, 0x10, 0xc, 0x4, 0x0,
	0x5f, 0xcf, 0xb3, 0xb3, 0xb3, 0x1e, 0x54, 0x0, 0x1, 0x8f, 0xb6, 0xb6, 0xb6, 0x1c, 0xb3, 0xb3,
	0xb3, 0xe4, 0x54, 0x0, 0x20, 0x4, 0x4, 0x0, 0x5c, 0xe3, 0xb3, 0xb3, 0xb3, 0x1b, 0xfc, 0x0,
	0x4f, 0xb3, 0xb3, 0xb3, 0xcd, 0x54, 0x0, 0x28, 0x1, 0x4, 0x0, 0x84, 0xb2, 0xb2, 0xb2, 0xcb,
	0xbf, 0xbf, 0xbf, 0x8, 0x58, 0x0, 0x4f, 0xb4, 0xb4, 0xb4, 0x7d, 0x54, 0x0, 0x2d, 0x7, 0x4,
	0x0, 0x14, 0x78, 0xf8, 0x1, 0xe, 0xa4, 0x1, 0xf, 0x4, 0x0, 0x2a, 0x10, 0xf0, 0x28, 0x2,
	0x4f, 0xb2, 0xb2, 0xb2, 0x63, 0x54, 0x0, 0x38, 0x4, 0x10, 0x1, 0x13, 0x60, 0xa0, 0x2, 0x1f,
	0xff, 0x4, 0x0, 0x3c, 0x5f, 0xae, 0xb3, 0xb3, 0xb3, 0xdb, 0x58, 0x0, 0x40, 0x0, 0x3c, 0x3,
	0x1f, 0xf3, 0x58, 0x0, 0x40, 0x1f, 0xf3, 0x58, 0x0, 0x44, 0x0, 0x8, 0x2, 0x1f, 0xd9, 0x58,
	0x0, 0x40, 0x13, 0xd7, 0x44, 0x4, 0x9, 0x18, 0x0, 0x7f, 0xbd, 0xbd, 0xbd, 0xff, 0xc6, 0xc6,
	0xc6, 0x4, 0x0, 0x12, 0x0, 0x2c, 0x0, 0x1e, 0xb3, 0x10, 0x2, 0x10, 0xad, 0x98, 0x4, 0xc,
	0x18, 0x0, 0x4f, 0xd9, 0xd9, 0xd9, 0xff, 0x1, 0x0, 0x15, 0x0, 0x2c, 0x0, 0x1e, 0xb3, 0xb0,
	0x0, 0x13, 0x5e, 0x18, 0x3, 0x18, 0xf0, 0x18, 0x0, 0x31, 0xb8, 0xb8, 0xb8, 0x88, 0x0, 0xf,
	0x4, 0x0, 0x11, 0x0, 0x2c, 0x0, 0xb, 0x54, 0x0, 0x53, 0xef, 0xaf, 0xaf, 0xaf, 0x10, 0xc8,
	0x3, 0x1f, 0x7a, 0x5c, 0x1, 0x38, 0x14, 0x76, 0x20, 0x4, 0x0, 0x2c, 0x4, 0x0, 0x34, 0x4,
	0xf, 0x54, 0x0, 0x30, 0x18, 0xc9, 0x78, 0x4, 0x0, 0x1, 0x0, 0x0, 0xe4, 0x4, 0x3, 0xec,
	0x4, 0xf, 0x54, 0x0, 0x25, 0x10, 0xe2, 0xc0, 0x5, 0xf, 0xd8, 0x5, 0x8, 0x1f, 0xc9, 0x54,
	0x0, 0x20, 0x1f, 0xce, 0xd8, 0x5, 0x5, 0x8, 0x14, 0x1, 0x0, 0xec, 0x4, 0xc, 0xcc, 0x1,
	0xf, 0x4, 0x0, 0x4, 0x10, 0xef, 0x50, 0x1, 0x4f, 0xb6, 0xb6, 0xb6, 0x7, 0xe8, 0x6, 0x15,
	0x3, 0xec, 0x4, 0x13, 0x60, 0xe0, 0x2, 0x17, 0xd7, 0x9c, 0x3, 0x10, 0xd7, 0x58, 0x4, 0x44,
	0xb4, 0xb4, 0xb4, 0x5f, 0xf8, 0x1, 0xf, 0x1, 0x0, 0x1, 0xf0, 0x0, 0x66, 0x68, 0x65, 0x69,
	0x67, 0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconRestore[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0xff, 0xc5, 0x8f, 0x65, 0x65, 0x65, 0x30, 0x68, 0x68, 0x68,
	0x40, 0x4, 0x0, 0x8, 0x1f, 0x20, 0x58, 0x0, 0x21, 0x8f, 0x67, 0x67, 0x67, 0x54, 0x66, 0x66,
	0x66, 0x70, 0x4, 0x0, 0x1, 0x40, 0x65, 0x65, 0x65, 0x7e, 0x5c, 0x0, 0xf, 0xcc, 0x0, 0x3d,
	0x0, 0x5c, 0x0, 0xf, 0x58, 0x0, 0x18, 0x0, 0xfc, 0x0, 0x5f, 0x60, 0x65, 0x65, 0x65, 0xc0,
	0x4, 0x0, 0x8, 0x1f, 0x90, 0x58, 0x0, 0x20, 0x0, 0x30, 0x0, 0x9c, 0x80, 0x65, 0x65, 0x65,
	0xf4, 0x64, 0x64, 0x64, 0xa0, 0x4, 0x0, 0x40, 0x65, 0x65, 0x65, 0xdc, 0x5c, 0x0, 0xf, 0x58,
	0x0, 0x28, 0x1f, 0xe0, 0x1c, 0x0, 0x1, 0x3, 0x5c, 0x0, 0xf, 0x58, 0x0, 0xfa, 0x0, 0x84,
	0x2, 0x0, 0xe0, 0x2, 0xf, 0x58, 0x0, 0x3d, 0xf, 0x98, 0x2, 0x20, 0x0, 0x58, 0x0, 0x10,
	0xfc, 0x5c, 0x0, 0x1e, 0x65, 0x4, 0x0, 0x1f, 0xf4, 0x58, 0x0, 0x24, 0x0, 0x94, 0x0, 0x10,
	0x40, 0x5c, 0x0, 0xf, 0x4, 0x0, 0x5, 0x0, 0xe0, 0x2, 0xf, 0xb8, 0x5, 0xff, 0xc6, 0xf0,
	0x0, 0x66, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16, 0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

static const uint8_t s_iconRestoreActive[] = {
	0x4c, 0x5a, 0x34, 0x53, 0xab, 0x7, 0xdf, 0xd9, 0xd9, 0xf7, 0xa3, 0x64, 0x64, 0x61, 0x74, 0x61,
	0x59, 0x7, 0x90, 0x0, 0x1, 0x0, 0x4, 0xf3, 0x5, 0xb8, 0xb8, 0xb8, 0x12, 0xb1, 0xb1, 0xb1,
	0x62, 0xb2, 0xb2, 0xb2, 0xb0, 0xb2, 0xb2, 0xb2, 0xd8, 0xb2, 0xb2, 0xb2, 0xf4, 0x4, 0x0, 0x0,
	0xc, 0x0, 0xaf, 0xaf, 0xb3, 0xb3, 0xb3, 0x61, 0xb4, 0xb4, 0xb4, 0x11, 0x0, 0x1, 0x0, 0x14,
	0xff, 0x1, 0xaa, 0xaa, 0xaa, 0x9, 0xb2, 0xb2, 0xb2, 0x7b, 0xb2, 0xb2, 0xb2, 0xf1, 0xb2, 0xb2,
	0xb2, 0xff, 0x4, 0x0, 0x9, 0x80, 0xb3, 0xb3, 0xb3, 0xf0, 0xb2, 0xb2, 0xb2, 0x7a, 0x34, 0x0,
	0xf, 0x54, 0x0, 0x9, 0x8f, 0xb1, 0xb1, 0xb1, 0x1a, 0xb2, 0xb2, 0xb2, 0xca, 0x50, 0x0, 0xd,
	0xc, 0x4, 0x0, 0x8f, 0xb1, 0xb1, 0xb1, 0xcf, 0xb3, 0xb3, 0xb3, 0x1e, 0x54, 0x0, 0x1, 0x8f,
	0xb6, 0xb6, 0xb6, 0x1c, 0xb2, 0xb2, 0xb2, 0xe4, 0x54, 0x0, 0x1d, 0x7, 0x5c, 0x0, 0x6e, 0xe3,
	0xb3, 0xb3, 0xb3, 0x1b, 0x0, 0xfc, 0x0, 0x1f, 0xcd, 0x54, 0x0, 0x25, 0x7, 0x4, 0x0, 0x57,
	0xcb, 0xbf, 0xbf, 0xbf, 0x8, 0x0, 0x1, 0x1f, 0x7d, 0x2c, 0x0, 0x5, 0x7f, 0xc0, 0xc0, 0xc0,
	0xff, 0xc5, 0xc5, 0xc5, 0x4, 0x0, 0x6, 0x3c, 0xbc, 0xbc, 0xbc, 0x74, 0x1, 0x14, 0x78, 0xf8,
	0x1, 0xf, 0xa4, 0x1, 0x9, 0x7f, 0xcb, 0xcb, 0xcb, 0xff, 0xd4, 0xd4, 0xd4, 0x4, 0x0, 0x2,
	0x31, 0xd8, 0xd8, 0xd8, 0x5c, 0x0, 0xc, 0xcc, 0x1, 0x0, 0x28, 0x2, 0x4f, 0xb2, 0xb2, 0xb2,
	0x63, 0xf8, 0x0, 0x25, 0x0, 0x5c, 0x0, 0xc, 0x58, 0x0, 0x3, 0x4, 0x0, 0x13, 0x60, 0xa0,
	0x2, 0x9, 0x18, 0x0, 0x7f, 0xcf, 0xcf, 0xcf, 0xff, 0xec, 0xec, 0xec, 0x4, 0x0, 0x6, 0x3e,
	0xdd, 0xdd, 0xdd, 0x58, 0x0, 0xa, 0xc4, 0x1, 0x5c, 0xae, 0xb2, 0xb2, 0xb2, 0xdb, 0x18, 0x0,
	0xbd, 0xd9, 0xd9, 0xd9, 0xff, 0xfc, 0xfc, 0xfc, 0xff, 0xe2, 0xe2, 0xe2, 0x4, 0x0, 0x31, 0xf4,
	0xf4, 0xf4, 0x5c, 0x0, 0xf, 0xb0, 0x0, 0xc, 0x0, 0x3c, 0x3, 0x1f, 0xf3, 0x58, 0x0, 0x1,
	0x3f, 0xf6, 0xf6, 0xf6, 0xfc, 0x0, 0x2, 0x0, 0x5c, 0x0, 0xf, 0x58, 0x0, 0x10, 0x1f, 0xf3,
	0x58, 0x0, 0x44, 0x5f, 0xf1, 0xb1, 0xb1, 0xb1, 0xd9, 0x58, 0x0, 0x40, 0x10, 0xd7, 0x44, 0x4,
	0xf, 0x58, 0x0, 0x25, 0x0, 0x84, 0x2, 0x0, 0xe0, 0x2, 0x1e, 0xb2, 0x28, 0x0, 0x10, 0xad,
	0x98, 0x4, 0xf, 0x58, 0x0, 0x25, 0xf, 0x90, 0x4, 0x8, 0x10, 0x5e, 0x18, 0x3, 0x3, 0x98,
	0x4, 0x9, 0x58, 0x0, 0x31, 0xfe, 0xfe, 0xfe, 0x5c, 0x0, 0xc, 0x4, 0x0, 0x0, 0x28, 0x2,
	0xf, 0x58, 0x0, 0xc, 0x50, 0xef, 0xaf, 0xaf, 0xaf, 0x10, 0x74, 0x3, 0x0, 0xec, 0x4, 0x8,
	0x1c, 0x0, 0x0, 0x34, 0x1, 0x0, 0x5c, 0x0, 0xf, 0x4, 0x0, 0x5, 0x0, 0xe0, 0x2, 0xf,
	0x80, 0x4, 0x8, 0x14, 0x76, 0x20, 0x4, 0x0, 0x2c, 0x4, 0x0, 0x34, 0x4, 0xf, 0x78, 0x4,
	0x30, 0x18, 0xc9, 0x78, 0x4, 0x0, 0x1, 0x0, 0x0, 0xe4, 0x4, 0x0, 0xec, 0x4, 0xf, 0x54,
	0x0, 0x28, 0x10, 0xe2, 0xc0, 0x5, 0xf, 0xd8, 0x5, 0x8, 0x1f, 0xc9, 0x54, 0x0, 0x20, 0x1f,
	0xce, 0xd8, 0x5, 0x5, 0x8, 0x14, 0x1, 0x0, 0xec, 0x4, 0xc, 0xcc, 0x1, 0xf, 0xa4, 0x1,
	0x5, 0x0, 0x50, 0x1, 0x4f, 0xb6, 0xb6, 0xb6, 0x7, 0xe8, 0x6, 0x15, 0x3, 0xec, 0x4, 0x13,
	0x60, 0xe0, 0x2, 0x17, 0xd7, 0x9c, 0x3, 0x10, 0xd7, 0x58, 0x4, 0x44, 0xb1, 0xb1, 0xb1, 0x5f,
	0xf8, 0x1, 0xf, 0x1, 0x0, 0x1, 0xf0, 0x0, 0x66, 0x68, 0x65, 0x69, 0x67, 0x68, 0x74, 0x16,
	0x65, 0x77, 0x69, 0x64, 0x74, 0x68, 0x16,
};

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

	if (_cursor) {
		dlclose(_cursor);
		_cursor = nullptr;
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

bool WaylandLibrary::ownsProxy(wl_proxy *proxy) {
	auto tag = getInstance()->wl_proxy_get_tag(proxy);
	return tag == &s_XenolithWaylandTag;
}

bool WaylandLibrary::ownsProxy(wl_output *output) {
	return ownsProxy((struct wl_proxy *) output);
}

bool WaylandLibrary::ownsProxy(wl_surface *surface) {
	return ownsProxy((struct wl_proxy *) surface);
}

bool WaylandLibrary::open(void *handle) {
	this->wl_registry_interface = reinterpret_cast<decltype(this->wl_registry_interface)>(dlsym(handle, "wl_registry_interface"));
	this->wl_compositor_interface = reinterpret_cast<decltype(this->wl_compositor_interface)>(dlsym(handle, "wl_compositor_interface"));
	this->wl_output_interface = reinterpret_cast<decltype(this->wl_output_interface)>(dlsym(handle, "wl_output_interface"));
	this->wl_seat_interface = reinterpret_cast<decltype(this->wl_seat_interface)>(dlsym(handle, "wl_seat_interface"));
	this->wl_surface_interface = reinterpret_cast<decltype(this->wl_surface_interface)>(dlsym(handle, "wl_surface_interface"));
	this->wl_region_interface = reinterpret_cast<decltype(this->wl_region_interface)>(dlsym(handle, "wl_region_interface"));
	this->wl_callback_interface = reinterpret_cast<decltype(this->wl_callback_interface)>(dlsym(handle, "wl_callback_interface"));
	this->wl_pointer_interface = reinterpret_cast<decltype(this->wl_callback_interface)>(dlsym(handle, "wl_pointer_interface"));
	this->wl_keyboard_interface = reinterpret_cast<decltype(this->wl_keyboard_interface)>(dlsym(handle, "wl_keyboard_interface"));
	this->wl_touch_interface = reinterpret_cast<decltype(this->wl_touch_interface)>(dlsym(handle, "wl_touch_interface"));
	this->wl_shm_interface = reinterpret_cast<decltype(this->wl_shm_interface)>(dlsym(handle, "wl_shm_interface"));
	this->wl_subcompositor_interface = reinterpret_cast<decltype(this->wl_subcompositor_interface)>(dlsym(handle, "wl_subcompositor_interface"));
	this->wl_subsurface_interface = reinterpret_cast<decltype(this->wl_subsurface_interface)>(dlsym(handle, "wl_subsurface_interface"));
	this->wl_shm_pool_interface = reinterpret_cast<decltype(this->wl_shm_pool_interface)>(dlsym(handle, "wl_shm_pool_interface"));
	this->wl_buffer_interface = reinterpret_cast<decltype(this->wl_buffer_interface)>(dlsym(handle, "wl_buffer_interface"));

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
	this->wl_proxy_set_user_data = reinterpret_cast<decltype(this->wl_proxy_set_user_data)>(dlsym(handle, "wl_proxy_set_user_data"));
	this->wl_proxy_get_user_data = reinterpret_cast<decltype(this->wl_proxy_get_user_data)>(dlsym(handle, "wl_proxy_get_user_data"));
	this->wl_proxy_set_tag = reinterpret_cast<decltype(this->wl_proxy_set_tag)>(dlsym(handle, "wl_proxy_set_tag"));
	this->wl_proxy_get_tag = reinterpret_cast<decltype(this->wl_proxy_get_tag)>(dlsym(handle, "wl_proxy_get_tag"));
	this->wl_proxy_destroy = reinterpret_cast<decltype(this->wl_proxy_destroy)>(dlsym(handle, "wl_proxy_destroy"));
	this->wl_display_roundtrip = reinterpret_cast<decltype(this->wl_display_roundtrip)>(dlsym(handle, "wl_display_roundtrip"));

	if (this->wl_registry_interface
			&& this->wl_compositor_interface
			&& this->wl_output_interface
			&& this->wl_seat_interface
			&& this->wl_surface_interface
			&& this->wl_region_interface
			&& this->wl_callback_interface
			&& this->wl_pointer_interface
			&& this->wl_keyboard_interface
			&& this->wl_touch_interface
			&& this->wl_shm_interface
			&& this->wl_subcompositor_interface
			&& this->wl_subsurface_interface
			&& this->wl_shm_pool_interface
			&& this->wl_buffer_interface
			&& this->wl_display_connect
			&& this->wl_display_get_fd
			&& this->wl_display_dispatch
			&& this->wl_display_disconnect
			&& this->wl_proxy_marshal_flags
			&& this->wl_proxy_get_version
			&& this->wl_proxy_add_listener
			&& this->wl_proxy_set_user_data
			&& this->wl_proxy_get_user_data
			&& this->wl_proxy_set_tag
			&& this->wl_proxy_get_tag
			&& this->wl_proxy_destroy
			&& this->wl_display_roundtrip
			&& this->wl_registry_interface) {
		viewporter = new ViewporterInterface(wl_surface_interface);
		xdg = new XdgInterface(wl_output_interface, wl_seat_interface, wl_surface_interface);

		wp_viewporter_interface = &viewporter->wp_viewporter_interface;
		wp_viewport_interface = &viewporter->wp_viewport_interface;

		xdg_wm_base_interface = &xdg->xdg_wm_base_interface;
		xdg_positioner_interface = &xdg->xdg_positioner_interface;
		xdg_surface_interface = &xdg->xdg_surface_interface;
		xdg_toplevel_interface = &xdg->xdg_toplevel_interface;
		xdg_popup_interface = &xdg->xdg_popup_interface;

		if (auto d = dlopen("libwayland-cursor.so", RTLD_LAZY)) {
			if (openWaylandCursor(d)) {
				_cursor = d;
			} else {
				dlclose(d);
			}
		}

		_handle = handle;
		return true;
	}
	return false;
}

bool WaylandLibrary::openWaylandCursor(void *d) {
	this->wl_cursor_theme_load = reinterpret_cast<decltype(this->wl_cursor_theme_load)>(dlsym(d, "wl_cursor_theme_load"));
	this->wl_cursor_theme_destroy = reinterpret_cast<decltype(this->wl_cursor_theme_destroy)>(dlsym(d, "wl_cursor_theme_destroy"));
	this->wl_cursor_theme_get_cursor = reinterpret_cast<decltype(this->wl_cursor_theme_get_cursor)>(dlsym(d, "wl_cursor_theme_get_cursor"));
	this->wl_cursor_image_get_buffer = reinterpret_cast<decltype(this->wl_cursor_image_get_buffer)>(dlsym(d, "wl_cursor_image_get_buffer"));

	return this->wl_cursor_theme_load
		&& this->wl_cursor_theme_destroy
		&& this->wl_cursor_theme_get_cursor
		&& this->wl_cursor_image_get_buffer;
}

void WaylandLibrary::openConnection(ConnectionData &data) {
	data.display = wl_display_connect(NULL);
}

static const xdg_wm_base_listener s_XdgWmBaseListener{
	[] (void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
		((WaylandDisplay *)data)->wayland->xdg_wm_base_pong(xdg_wm_base, serial);
	},
};

static const wl_registry_listener s_WaylandRegistryListener{
	[] (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
		auto display = (WaylandDisplay *)data;
		auto wayland = display->wayland.get();

		XL_WAYLAND_LOG("handleGlobal: '", interface, "', version: ", version, ", name: ", name);
		if (strcmp(interface, wayland->wl_compositor_interface->name) == 0) {
			display->compositor = static_cast<wl_compositor *>(wayland->wl_registry_bind(registry, name,
					wayland->wl_compositor_interface, std::min(version, 4U)));
		} else if (strcmp(interface, wayland->wl_subcompositor_interface->name) == 0) {
			display->subcompositor = static_cast<wl_subcompositor *>(wayland->wl_registry_bind(registry, name,
					wayland->wl_subcompositor_interface, 1U));
		} else if (strcmp(interface, wayland->wl_output_interface->name) == 0) {
			auto out = Rc<WaylandOutput>::create(wayland, registry, name, version);
			display->outputs.emplace_back(move(out));
		} else if (strcmp(interface, wayland->wp_viewporter_interface->name) == 0) {
			display->viewporter = static_cast<struct wp_viewporter *>(wayland->wl_registry_bind(registry, name,
					wayland->wp_viewporter_interface, 1));
		} else if (strcmp(interface, wayland->xdg_wm_base_interface->name) == 0) {
			display->xdgWmBase = static_cast<struct xdg_wm_base *>(wayland->wl_registry_bind(registry, name,
					wayland->xdg_wm_base_interface, std::min(version, 2U)));
			wayland->xdg_wm_base_add_listener(display->xdgWmBase, &s_XdgWmBaseListener, display);
		} else if (strcmp(interface, wayland->wl_shm_interface->name) == 0) {
			display->shm = Rc<WaylandShm>::create(wayland, registry, name, version);
		} else if (strcmp(interface, wayland->wl_seat_interface->name) == 0) {
			display->seat = Rc<WaylandSeat>::create(wayland, display, registry, name, version);
		}
	},
	[] (void *data, struct wl_registry *registry, uint32_t name) {
		//auto display = (WaylandDisplay *)data;

	},
};

WaylandDisplay::~WaylandDisplay() {
	if (xdgWmBase) {
		wayland->xdg_wm_base_destroy(xdgWmBase);
		xdgWmBase = nullptr;
	}
	if (compositor) {
		wayland->wl_compositor_destroy(compositor);
		compositor = nullptr;
	}
	if (subcompositor) {
		wayland->wl_subcompositor_destroy(subcompositor);
		subcompositor = nullptr;
	}
	if (viewporter) {
		wayland->wp_viewporter_destroy(viewporter);
		viewporter = nullptr;
	}

	shm = nullptr;
	seat = nullptr;
	outputs.clear();

	if (display) {
		wayland->wl_display_disconnect(display);
		display = nullptr;
	}
}

bool WaylandDisplay::init(const Rc<WaylandLibrary> &lib) {
	wayland = lib;

	auto connection = wayland->acquireConnection();

	display = connection.display;

	struct wl_registry *registry = wayland->wl_display_get_registry(display);

	wayland->wl_registry_add_listener(registry, &s_WaylandRegistryListener, this);
	wayland->wl_display_roundtrip(display); // registry
	wayland->wl_display_roundtrip(display); // seats and outputs
	wayland->wl_registry_destroy(registry);

	xkb = XkbLibrary::getInstance();
	return true;
}

wl_surface *WaylandDisplay::createSurface(WaylandView *view) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, view);
	wayland->wl_proxy_set_tag((struct wl_proxy *) surface, &s_XenolithWaylandTag);
	surfaces.emplace(surface);
	return surface;
}

void WaylandDisplay::destroySurface(wl_surface *surface) {
	surfaces.erase(surface);
	wayland->wl_surface_destroy(surface);
}

wl_surface *WaylandDisplay::createDecorationSurface(WaylandDecoration *decor) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, decor);
	wayland->wl_proxy_set_tag((struct wl_proxy *) surface, &s_XenolithWaylandTag);
	decorations.emplace(surface);
	return surface;
}

void WaylandDisplay::destroyDecorationSurface(wl_surface *surface) {
	decorations.erase(surface);
	wayland->wl_surface_destroy(surface);
}

bool WaylandDisplay::ownsSurface(wl_surface *surface) const {
	return surfaces.find(surface) != surfaces.end();
}

bool WaylandDisplay::isDecoration(wl_surface *surface) const {
	return decorations.find(surface) != decorations.end();
}

bool WaylandDisplay::flush() {
	while (wayland->wl_display_prepare_read(display) != 0) {
		wayland->wl_display_dispatch_pending(display);
	}

	while (wayland->wl_display_flush(display) == -1) {
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

	wayland->wl_display_read_events(display);
	wayland->wl_display_dispatch_pending(display);

	return true;
}

int WaylandDisplay::getSocketFd() const {
	return wayland->wl_display_get_fd(display);
}

WaylandCursorTheme::~WaylandCursorTheme() {
	if (cursorTheme) {
		wayland->wl_cursor_theme_destroy(cursorTheme);
		cursorTheme = nullptr;
	}
}

static const char *getCursorName(WaylandCursorImage image) {
	switch (image) {
	case WaylandCursorImage::LeftPtr: return "left_ptr"; break;
	case WaylandCursorImage::EResize: return "right_side"; break;
	case WaylandCursorImage::NEResize: return "top_right_corner"; break;
	case WaylandCursorImage::NResize: return "top_side"; break;
	case WaylandCursorImage::NWResize: return "top_left_corner"; break;
	case WaylandCursorImage::SEResize: return "bottom_right_corner"; break;
	case WaylandCursorImage::SResize: return "bottom_side"; break;
	case WaylandCursorImage::SWResize: return "bottom_left_corner"; break;
	case WaylandCursorImage::WResize: return "left_side"; break;
	case WaylandCursorImage::Max: break;
	}
	return nullptr;
}

bool WaylandCursorTheme::init(WaylandDisplay *display, const char *name, int size) {
	wayland = display->wayland;
	cursorSize = size;
	if (name) {
		cursorName = name;
	}
	cursorTheme = wayland->wl_cursor_theme_load(name, size, display->shm->shm);

	if (cursorTheme) {
		for (int i = 0; i < toInt(WaylandCursorImage::Max); ++ i) {
			auto name = getCursorName(WaylandCursorImage(i));
			auto cursor = wayland->wl_cursor_theme_get_cursor(cursorTheme, name);
			cursors.emplace_back(cursor);
		}
		return true;
	}

	return false;
}

void WaylandCursorTheme::setCursor(WaylandSeat *seat) {
	setCursor(seat->pointer, seat->cursorSurface, seat->serial, seat->cursorImage, seat->pointerScale);
}

void WaylandCursorTheme::setCursor(wl_pointer *pointer, wl_surface *cursorSurface, uint32_t serial, WaylandCursorImage img, int scale) {
	if (!cursorTheme || cursors.size() <= size_t(toInt(img))) {
		return;
	}

	auto cursor = cursors[toInt(img)];
	if (!cursor) {
		cursor = cursors[0];
	}

	auto image = cursor->images[0];
	auto buffer = wayland->wl_cursor_image_get_buffer(image);
	wayland->wl_pointer_set_cursor(pointer, serial, cursorSurface, image->hotspot_x / scale, image->hotspot_y / scale);
	wayland->wl_surface_attach(cursorSurface, buffer, 0, 0);
	wayland->wl_surface_set_buffer_scale(cursorSurface, scale);
	wayland->wl_surface_damage_buffer(cursorSurface, 0, 0, image->width, image->height);
	wayland->wl_surface_commit(cursorSurface);
}

static struct wl_shm_listener s_WaylandShmListener = {
	[] (void *data, wl_shm *shm, uint32_t format) {
		((WaylandShm *)data)->format = format;
	}
};

static int createAnonymousFile(off_t size) {
    static const char tpl[] = "/xl-wayland-XXXXXX";
    const char* path;
    int fd;
    int ret;

	fd = ::memfd_create("xl-wayland", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd >= 0) {
		::fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
	} else {
		path = getenv("XDG_RUNTIME_DIR");
		if (!path) {
			errno = ENOENT;
			return -1;
		}

		char* tmpname = (char *)::calloc(strlen(path) + sizeof(tpl), 1);
		::strcpy(tmpname, path);
		::strcat(tmpname, tpl);

		fd = ::mkostemp(tmpname, O_CLOEXEC);
		::free(tmpname);
		if (fd >= 0) {
			::unlink(tmpname);
		} else {
			return -1;
		}
	}

    ret = ::posix_fallocate(fd, 0, size);
	if (ret != 0) {
		::close(fd);
		errno = ret;
		return -1;
	}
    return fd;
}

WaylandBuffer::~WaylandBuffer() {
	if (buffer) {
		wayland->wl_buffer_destroy(buffer);
		buffer = nullptr;
	}
}

bool WaylandBuffer::init(WaylandLibrary *lib, wl_shm_pool *pool, int32_t offset,
		int32_t w, int32_t h, int32_t stride, uint32_t format) {
	wayland = lib;
	width = w;
	height = h;
	buffer = wayland->wl_shm_pool_create_buffer(pool, offset, width, height, stride, format);
	return true;
}

WaylandShm::~WaylandShm() {
	if (shm) {
		wayland->wl_shm_destroy(shm);
		shm = nullptr;
	}
}

bool WaylandShm::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name, uint32_t version) {
	wayland = lib;
	id = name;
	shm = static_cast<wl_shm*>(wayland->wl_registry_bind(registry, name,
			wayland->wl_shm_interface, std::min(version, 1U)));
	wayland->wl_shm_set_user_data(shm, this);
	wayland->wl_shm_add_listener(shm, &s_WaylandShmListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *) shm, &s_XenolithWaylandTag);
	return true;
}

static void makeGaussianVector(Color4B *retA, Color4B *retB, uint32_t size) {
	const float sigma = sqrtf((size * size) / (-2.0f * logf(1.0f / 255.0f)));
	const float sigma_v = -1.0f / (2.0f * sigma * sigma);

	for (uint32_t j = 0; j < size; j++) {
		retA[j].a = (uint8_t)(24.0f * expf( (j * j) * sigma_v ));
		retB[j].a = (uint8_t)(64.0f * expf( (j * j) * sigma_v ));
	}
}

static void makeGaussianRange(Color4B *retA, Color4B *retB, uint32_t size, uint32_t inset) {
	auto width = size + inset;
	auto targetA = retA;
	auto targetB = retA + width * width;
	auto targetC = retA + width * width * 2;
	auto targetD = retA + width * width * 3;
	auto targetE = retB;
	auto targetF = retB + width * width;
	auto targetG = retB + width * width * 2;
	auto targetH = retB + width * width * 3;

	const float sigma = sqrtf((size * size) / (-2.0f * logf(1.0f / 255.0f)));
	const float sigma_v = -1.0f / (2.0f * sigma * sigma);

	for (uint32_t i = 0; i < width; i++) {
		for (uint32_t j = 0; j < width; j++) {
			float dist = sqrtf( (i * i) + (j * j) );
			float tmp = 0.0f;
			if (dist <= inset) {
				tmp = 1.0f;
			} else if (dist > size + inset) {
				tmp = 0.0f;
			} else {
				dist = dist - inset;
				tmp = expf( (dist * dist) * sigma_v);
			}

			auto valueA = (uint8_t)(24.0f * tmp);
			auto valueB = (uint8_t)(64.0f * tmp);
			targetA[i * width + j].a = valueA;
			targetB[(width - i - 1) * width + (width - j - 1)].a = valueA;
			targetC[(i) * width + (width - j - 1)].a = valueA;
			targetD[(width - i - 1) * width + (j)].a = valueA;
			targetE[i * width + j].a = valueB;
			targetF[(width - i - 1) * width + (width - j - 1)].a = valueB;
			targetG[(i) * width + (width - j - 1)].a = valueB;
			targetH[(width - i - 1) * width + (j)].a = valueB;
		}
	}
}

static void makeRoundedHeader(Color4B *retA, uint32_t size, const Color3B &colorA, const Color3B &colorB) {
	auto targetA = retA;
	auto targetB = retA + size * size * 1;
	auto targetC = retA + size * size * 2;
	auto targetD = retA + size * size * 3;

	Color4B tmpA = Color4B(colorA.b, colorA.g, colorA.r, 255);
	Color4B tmpB = Color4B(colorB.b, colorB.g, colorB.r, 255);
	for (uint32_t i = 0; i < size; i++) {
		for (uint32_t j = 0; j < size; j++) {
			auto u = size - i - 1;
			auto v = size - j - 1;
			float dist = sqrtf( (u * u) + (v * v) );
			if (dist >= size) {
				targetA[i * size + j] = Color4B(0, 0, 0, 0);
				targetB[i * size + j] = Color4B(0, 0, 0, 0);
				targetC[i * size + (size - j - 1)] = Color4B(0, 0, 0, 0);
				targetD[i * size + (size - j - 1)] = Color4B(0, 0, 0, 0);
			} else {
				targetA[i * size + j] = tmpA;
				targetB[i * size + j] = tmpB;
				targetC[i * size + (size - j - 1)] = tmpA;
				targetD[i * size + (size - j - 1)] = tmpB;
			}
		}
	}
}

bool WaylandShm::allocateDecorations(ShadowBuffers *ret, uint32_t width, uint32_t inset,
		const Color3B &header, const Color3B &headerActive) {

	auto size = width * sizeof(Color4B) * 8; // plain shadows
	size += (width + inset) * (width + inset) * sizeof(Color4B) * 8; // cornerShadows
	size += (inset * 2 * inset) * sizeof(Color4B) * 2;
	size += 2 * sizeof(Color4B);

	struct IconData {
		WaylandDecorationName name;
		Value value;
		bool active;
		uint32_t width = 0;
		uint32_t height = 0;
		BytesView data;
	};

	Vector<IconData> icons;

	auto loadData = [&] (WaylandDecorationName name, BytesView data, bool active) {
		auto &icon = icons.emplace_back(IconData{name, data::read<Interface>(data), active});
		icon.width = icon.value.getInteger("width");
		icon.height = icon.value.getInteger("height");
		icon.data = icon.value.getBytes("data");
		return icon.width * icon.height * sizeof(Color4B);
	};

	size += loadData(WaylandDecorationName::IconClose, BytesView(s_iconClose, sizeof(s_iconClose)), false);
	size += loadData(WaylandDecorationName::IconMaximize, BytesView(s_iconMaximize, sizeof(s_iconMaximize)), false);
	size += loadData(WaylandDecorationName::IconMinimize, BytesView(s_iconMinimize, sizeof(s_iconMinimize)), false);
	size += loadData(WaylandDecorationName::IconRestore, BytesView(s_iconRestore, sizeof(s_iconRestore)), false);
	size += loadData(WaylandDecorationName::IconClose, BytesView(s_iconCloseActive, sizeof(s_iconCloseActive)), true);
	size += loadData(WaylandDecorationName::IconMaximize, BytesView(s_iconMaximizeActive, sizeof(s_iconMaximizeActive)), true);
	size += loadData(WaylandDecorationName::IconMinimize, BytesView(s_iconMinimizeActive, sizeof(s_iconMinimizeActive)), true);
	size += loadData(WaylandDecorationName::IconRestore, BytesView(s_iconRestoreActive, sizeof(s_iconRestoreActive)), true);

	const int fd = createAnonymousFile(size);
	if (fd < 0) {
		return false;
	}

	auto data = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		::close(fd);
		return false;
	}

	auto pool = wayland->wl_shm_create_pool(shm, fd, size);
	::close(fd);

	auto targetA = (Color4B *)data;
	auto targetB = (Color4B *)data + width * 4;
	makeGaussianVector(targetA, targetB, width);

	// make normal
	::memcpy(targetA + width * 2, targetA, width * sizeof(Color4B));
	::memcpy(targetA + width * 3, targetA, width * sizeof(Color4B));
	std::reverse(targetA, targetA + width);
	::memcpy(targetA + width * 1, targetA, width * sizeof(Color4B));

	// make active
	::memcpy(targetB + width * 2, targetB, width * sizeof(Color4B));
	::memcpy(targetB + width * 3, targetB, width * sizeof(Color4B));
	std::reverse(targetB, targetB + width);
	::memcpy(targetB + width * 1, targetB, width * sizeof(Color4B));

	ret->top = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 0,
			1, width, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->left = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 1,
			width, 1, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->bottom = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 2,
			1, width, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->right = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 3,
			width, 1, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->topActive = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 4,
			1, width, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->leftActive = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 5,
			width, 1, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->bottomActive = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 6,
			1, width, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->rightActive = Rc<WaylandBuffer>::create(wayland, pool, width * sizeof(Color4B) * 7,
			width, 1, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);

	auto offset = width * 8 * sizeof(Color4B);
	targetA = (Color4B *)data + width * 8;
	width += inset;
	targetB = targetA + width * width * 4;
	makeGaussianRange(targetA, targetB, width - inset, inset);

	ret->bottomRight = Rc<WaylandBuffer>::create(wayland, pool, offset,
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->topLeft = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->bottomLeft = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 2 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->topRight = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 3 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->bottomRightActive =Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 4 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->topLeftActive = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 5 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->bottomLeftActive = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 6 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->topRightActive = Rc<WaylandBuffer>::create(wayland, pool, offset + width * width * 7 * sizeof(Color4B),
			width, width, width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);

	offset += width * width * 8 * sizeof(Color4B);
	targetA = (Color4B *)((uint8_t *)data + offset);

	makeRoundedHeader(targetA, inset, header, headerActive);

	ret->headerLeft = Rc<WaylandBuffer>::create(wayland, pool, offset,
			inset, inset, inset * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->headerLeftActive = Rc<WaylandBuffer>::create(wayland, pool, offset + inset * inset * sizeof(Color4B) * 1,
			inset, inset, inset * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->headerRight = Rc<WaylandBuffer>::create(wayland, pool, offset + inset * inset * sizeof(Color4B) * 2,
			inset, inset, inset * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->headerRightActive = Rc<WaylandBuffer>::create(wayland, pool, offset + inset * inset * sizeof(Color4B) * 3,
			inset, inset, inset * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);

	offset += inset * inset * sizeof(Color4B) * 4;
	targetA = (Color4B *)((uint8_t *)data + offset);
	targetA[0] = Color4B(header.b, header.g, header.r, 255);
	targetA[1] = Color4B(headerActive.b, headerActive.g, headerActive.r, 255);

	ret->headerCenter = Rc<WaylandBuffer>::create(wayland, pool, offset,
			1, 1, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
	ret->headerCenterActive = Rc<WaylandBuffer>::create(wayland, pool, offset + sizeof(Color4B),
			1, 1, sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);

	offset += 2 * sizeof(Color4B);
	targetA += 2;

	for (IconData &it : icons) {
		memcpy((uint8_t *)data + offset, it.data.data(), it.data.size());
		Rc<WaylandBuffer> buf = Rc<WaylandBuffer>::create(wayland, pool, offset,
			it.width, it.height, it.width * sizeof(Color4B), WL_SHM_FORMAT_ARGB8888);
		switch (it.name) {
		case WaylandDecorationName::IconClose:
			if (it.active) { ret->iconCloseActive = move(buf); } else { ret->iconClose = move(buf); }
			break;
		case WaylandDecorationName::IconMaximize:
			if (it.active) { ret->iconMaximizeActive = move(buf); } else { ret->iconMaximize = move(buf); }
			break;
		case WaylandDecorationName::IconMinimize:
			if (it.active) { ret->iconMinimizeActive = move(buf); } else { ret->iconMinimize = move(buf); }
			break;
		case WaylandDecorationName::IconRestore:
			if (it.active) { ret->iconRestoreActive = move(buf); } else { ret->iconRestore = move(buf); }
			break;
		default:
			break;
		}
		offset += it.data.size();
	}

	::munmap(data, size);
	wayland->wl_shm_pool_destroy(pool);

	return true;
}

static const wl_output_listener s_WaylandOutputListener{
	// geometry
	[] (void *data, wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
			int32_t subpixel, char const *make, char const *model, int32_t transform) {
		auto out = ((WaylandOutput *)data);
		out->geometry = WaylandOutput::Geometry{x, y, physical_width, physical_height,
			subpixel, transform, make, model};
		if (out->name.empty()) {
			out->name = toString(make, " ", model);
		}
	},

	// mode
	[] (void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
		((WaylandOutput *)data)->mode = WaylandOutput::Mode{flags, width, height, refresh};
	},

	// done
	[] (void *data, wl_output* output) {
		// do nothing
	},

	// scale
	[] (void *data, wl_output* output, int32_t factor) {
		((WaylandOutput *)data)->scale = factor;
	},

	// name
	[] (void *data, struct wl_output *wl_output, const char *name) {
		((WaylandOutput *)data)->name = name;
	},

	// description
	[] (void *data, struct wl_output *wl_output, const char *name) {
		((WaylandOutput *)data)->desc = name;
	}
};

WaylandOutput::~WaylandOutput() {
	if (output) {
		wayland->wl_output_destroy(output);
		output = nullptr;
	}
}

bool WaylandOutput::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name, uint32_t version) {
	wayland = lib;
	id = name;
	output = static_cast<wl_output*>(wayland->wl_registry_bind(registry, name,
			wayland->wl_output_interface, std::min(version, 2U)));
	wayland->wl_output_set_user_data(output, this);
	wayland->wl_output_add_listener(output, &s_WaylandOutputListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *) output, &s_XenolithWaylandTag);
	return true;
}

String WaylandOutput::description() const {
	StringStream stream;
	stream << geometry.make << " " << geometry.model << ": "
			<< mode.width << "x" << mode.height << "@" << mode.refresh / 1000 << "Hz (x" << scale << ");";
	if (mode.flags & WL_OUTPUT_MODE_CURRENT) {
		stream << " Current;";
	}
	if (mode.flags & WL_OUTPUT_MODE_PREFERRED) {
		stream << " Preferred;";
	}
	if (!desc.empty()) {
		stream << " " << desc << ";";
	}
	return stream.str();
}

static struct wl_pointer_listener s_WaylandPointerListener{
	// enter
	[] (void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
		auto seat = (WaylandSeat *)data;
		seat->pointerFocus = surface;
		seat->serial = serial;

		if (!seat->root->ownsSurface(surface)) {
			if (seat->root->isDecoration(surface)) {
				if (auto decor = (WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
					if (decor->image != seat->cursorImage) {
						if (seat->cursorTheme) {
							seat->cursorImage = decor->image;
							seat->cursorTheme->setCursor(seat);
						}
					}
					seat->pointerDecorations.emplace(decor);
					decor->onEnter();
				}
			}
			return;
		}

		if (auto view = (WaylandView *)seat->wayland->wl_surface_get_user_data(surface)) {
			seat->pointerViews.emplace(view);
			if (WaylandCursorImage::LeftPtr != seat->cursorImage) {
				if (seat->cursorTheme) {
					seat->cursorImage = WaylandCursorImage::LeftPtr;
					seat->cursorTheme->setCursor(seat);
				}
			}
			view->handlePointerEnter(surface_x, surface_y);
		}
	},

	// leave
	[] (void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface) {
		auto seat = (WaylandSeat *)data;

		if (seat->root->isDecoration(surface)) {
			if (auto decor = (WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
				decor->waitForMove = false;
				seat->pointerDecorations.erase(decor);
				decor->onLeave();
			}
		}

		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandView *)seat->wayland->wl_surface_get_user_data(surface)) {
				view->handlePointerLeave();
				seat->pointerViews.erase(view);
			}
		}

		if (seat->pointerFocus == surface) {
			seat->pointerFocus = NULL;
			seat->cursorImage = WaylandCursorImage::Max;
		}
	},

	// motion
	[] (void *data, wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerMotion(time, surface_x, surface_y);
		}
		for (auto &it : seat->pointerDecorations) {
			it->handleMotion();
		}
	},

	// button
	[] (void *data, wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerButton(serial, time, button, state);
		}
		for (auto &it : seat->pointerDecorations) {
			it->handlePress(serial, button, state);
		}
	},

	// axis
	[] (void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerAxis(time, axis, value);
		}
	},

	// frame
	[] (void *data, wl_pointer *wl_pointer) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerFrame();
		}
	},

	// axis_source
	[] (void *data, wl_pointer *wl_pointer, uint32_t axis_source) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerAxisSource(axis_source);
		}
	},

	// axis_stop
	[] (void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerAxisStop(time, axis);
		}
	},

	// axis_discrete
	[] (void *data, wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->pointerViews) {
			it->handlePointerAxisDiscrete(axis, discrete);
		}
	}
};

static struct wl_keyboard_listener s_WaylandKeyboardListener{
	// keymap
	[] (void *data, wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->xkb) {
			auto map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
			if (map_shm != MAP_FAILED) {
				if (seat->state) {
					seat->root->xkb->xkb_state_unref(seat->state);
					seat->state = nullptr;
				}

				if (seat->compose) {
					seat->root->xkb->xkb_compose_state_unref(seat->compose);
					seat->compose = nullptr;
				}

				auto keymap = seat->root->xkb->xkb_keymap_new_from_string(seat->root->xkb->getContext(),
						(const char *)map_shm, xkb_keymap_format(format), XKB_KEYMAP_COMPILE_NO_FLAGS);
				if (keymap) {
					seat->state = seat->root->xkb->xkb_state_new(keymap);
					seat->keyState.controlIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Control");
					seat->keyState.altIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod1");
					seat->keyState.shiftIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Shift");
					seat->keyState.superIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod4");
					seat->keyState.capsLockIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Lock");
					seat->keyState.numLockIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod2");
					seat->root->xkb->xkb_keymap_unref(keymap);
				}

				auto locale = getenv("LC_ALL");
				if (!locale) { locale = getenv("LC_CTYPE"); }
				if (!locale) { locale = getenv("LANG"); }

				auto composeTable = seat->root->xkb->xkb_compose_table_new_from_locale(seat->root->xkb->getContext(),
						locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
				if (composeTable) {
					seat->compose = seat->root->xkb->xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
					seat->root->xkb->xkb_compose_table_unref(composeTable);
				}

				munmap(map_shm, size);
			}
		}
	    close(fd);
	},

	// enter
	[] (void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface, struct wl_array *keys) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandView *)seat->wayland->wl_surface_get_user_data(surface)) {
				Vector<uint32_t> keysVec;
				for (uint32_t *it = (uint32_t *)keys->data; (const char *)it < ((const char *) keys->data + keys->size); ++ it) {
					keysVec.emplace_back(*it);
				}

				seat->keyboardViews.emplace(view);
				view->handleKeyboardEnter(move(keysVec), seat->keyState.modsDepressed, seat->keyState.modsLatched, seat->keyState.modsLocked);
			}
		}
	},

	// leave
	[] (void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandView *)seat->wayland->wl_surface_get_user_data(surface)) {
				view->handleKeyboardLeave();
				seat->keyboardViews.erase(view);
			}
		}
	},

	// key
	[] (void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->keyboardViews) {
			it->handleKey(time, key, state);
		}
	},

	// modifiers
	[] (void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed,
			uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
		auto seat = (WaylandSeat *)data;
		if (seat->state) {
			seat->root->xkb->xkb_state_update_mask(seat->state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
			seat->keyState.modsDepressed = mods_depressed;
			seat->keyState.modsLatched = mods_latched;
			seat->keyState.modsLocked = mods_locked;
			for (auto &it : seat->keyboardViews) {
				it->handleKeyModifiers(mods_depressed, mods_latched, mods_locked);
			}
		}
	},

	// repeat_info
	[] (void *data, wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
		auto seat = (WaylandSeat *)data;
		seat->keyState.keyRepeatRate = rate;
		seat->keyState.keyRepeatDelay = delay;
		seat->keyState.keyRepeatInterval = 1'000'000 / rate;
	}
};

static struct wl_touch_listener s_WaylandTouchListener{
	// down
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, wl_surface *surface, int32_t id, wl_fixed_t x, wl_fixed_t y) {

	},

	// up
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, int32_t id) {

	},

	// motion
	[](void *data, wl_touch *touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {

	},

	// frame
	[](void *data, wl_touch *touch) {

	},

	// cancel
	[](void *data, wl_touch *touch) {

	},

	// shape
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t major, wl_fixed_t minor) {

	},

	// orientation
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t orientation) {

	}
};

static struct wl_seat_listener s_WaylandSeatListener{
	[] (void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
		auto seat = (WaylandSeat *)data;
		seat->capabilities = capabilities;
		seat->root->seatDirty = true;

		if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0) {
			seat->initCursors();
		}

		seat->update();
	},
	[] (void *data, struct wl_seat *wl_seat, const char *name) {
		auto seat = (WaylandSeat*) data;
		seat->name = name;
	}
};

WaylandSeat::~WaylandSeat() {
	if (state) {
		root->xkb->xkb_state_unref(state);
		state = nullptr;
	}
	if (compose) {
		root->xkb->xkb_compose_state_unref(compose);
		compose = nullptr;
	}
	if (seat) {
		wayland->wl_seat_destroy(seat);
		seat = nullptr;
	}
}

bool WaylandSeat::init(const Rc<WaylandLibrary> &lib, WaylandDisplay *view, wl_registry *registry, uint32_t name, uint32_t version) {
	wayland = lib;
	root = view;
	id = name;
	if (version >= 5U) {
		hasPointerFrames = true;
	}
	seat = static_cast<wl_seat *>(wayland->wl_registry_bind(registry, name,
			wayland->wl_seat_interface, std::min(version, 5U)));
	wayland->wl_seat_set_user_data(seat, this);
	wayland->wl_seat_add_listener(seat, &s_WaylandSeatListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *) seat, &s_XenolithWaylandTag);

	keyState.keycodes[KEY_GRAVE] = InputKeyCode::GRAVE_ACCENT;
	keyState.keycodes[KEY_1] = InputKeyCode::_1;
	keyState.keycodes[KEY_2] = InputKeyCode::_2;
	keyState.keycodes[KEY_3] = InputKeyCode::_3;
	keyState.keycodes[KEY_4] = InputKeyCode::_4;
	keyState.keycodes[KEY_5] = InputKeyCode::_5;
	keyState.keycodes[KEY_6] = InputKeyCode::_6;
	keyState.keycodes[KEY_7] = InputKeyCode::_7;
	keyState.keycodes[KEY_8] = InputKeyCode::_8;
	keyState.keycodes[KEY_9] = InputKeyCode::_9;
	keyState.keycodes[KEY_0] = InputKeyCode::_0;
	keyState.keycodes[KEY_SPACE] = InputKeyCode::SPACE;
	keyState.keycodes[KEY_MINUS] = InputKeyCode::MINUS;
	keyState.keycodes[KEY_EQUAL] = InputKeyCode::EQUAL;
	keyState.keycodes[KEY_Q] = InputKeyCode::Q;
	keyState.keycodes[KEY_W] = InputKeyCode::W;
	keyState.keycodes[KEY_E] = InputKeyCode::E;
	keyState.keycodes[KEY_R] = InputKeyCode::R;
	keyState.keycodes[KEY_T] = InputKeyCode::T;
	keyState.keycodes[KEY_Y] = InputKeyCode::Y;
	keyState.keycodes[KEY_U] = InputKeyCode::U;
	keyState.keycodes[KEY_I] = InputKeyCode::I;
	keyState.keycodes[KEY_O] = InputKeyCode::O;
	keyState.keycodes[KEY_P] = InputKeyCode::P;
	keyState.keycodes[KEY_LEFTBRACE] = InputKeyCode::LEFT_BRACKET;
	keyState.keycodes[KEY_RIGHTBRACE] = InputKeyCode::RIGHT_BRACKET;
	keyState.keycodes[KEY_A] = InputKeyCode::A;
	keyState.keycodes[KEY_S] = InputKeyCode::S;
	keyState.keycodes[KEY_D] = InputKeyCode::D;
	keyState.keycodes[KEY_F] = InputKeyCode::F;
	keyState.keycodes[KEY_G] = InputKeyCode::G;
	keyState.keycodes[KEY_H] = InputKeyCode::H;
	keyState.keycodes[KEY_J] = InputKeyCode::J;
	keyState.keycodes[KEY_K] = InputKeyCode::K;
	keyState.keycodes[KEY_L] = InputKeyCode::L;
	keyState.keycodes[KEY_SEMICOLON] = InputKeyCode::SEMICOLON;
	keyState.keycodes[KEY_APOSTROPHE] = InputKeyCode::APOSTROPHE;
	keyState.keycodes[KEY_Z] = InputKeyCode::Z;
	keyState.keycodes[KEY_X] = InputKeyCode::X;
	keyState.keycodes[KEY_C] = InputKeyCode::C;
	keyState.keycodes[KEY_V] = InputKeyCode::V;
	keyState.keycodes[KEY_B] = InputKeyCode::B;
	keyState.keycodes[KEY_N] = InputKeyCode::N;
	keyState.keycodes[KEY_M] = InputKeyCode::M;
	keyState.keycodes[KEY_COMMA] = InputKeyCode::COMMA;
	keyState.keycodes[KEY_DOT] = InputKeyCode::PERIOD;
	keyState.keycodes[KEY_SLASH] = InputKeyCode::SLASH;
	keyState.keycodes[KEY_BACKSLASH] = InputKeyCode::BACKSLASH;
	keyState.keycodes[KEY_ESC] = InputKeyCode::ESCAPE;
	keyState.keycodes[KEY_TAB] = InputKeyCode::TAB;
	keyState.keycodes[KEY_LEFTSHIFT] = InputKeyCode::LEFT_SHIFT;
	keyState.keycodes[KEY_RIGHTSHIFT] = InputKeyCode::RIGHT_SHIFT;
	keyState.keycodes[KEY_LEFTCTRL] = InputKeyCode::LEFT_CONTROL;
	keyState.keycodes[KEY_RIGHTCTRL] = InputKeyCode::RIGHT_CONTROL;
	keyState.keycodes[KEY_LEFTALT] = InputKeyCode::LEFT_ALT;
	keyState.keycodes[KEY_RIGHTALT] = InputKeyCode::RIGHT_ALT;
	keyState.keycodes[KEY_LEFTMETA] = InputKeyCode::LEFT_SUPER;
	keyState.keycodes[KEY_RIGHTMETA] = InputKeyCode::RIGHT_SUPER;
	keyState.keycodes[KEY_COMPOSE] = InputKeyCode::MENU;
	keyState.keycodes[KEY_NUMLOCK] = InputKeyCode::NUM_LOCK;
	keyState.keycodes[KEY_CAPSLOCK] = InputKeyCode::CAPS_LOCK;
	keyState.keycodes[KEY_PRINT] = InputKeyCode::PRINT_SCREEN;
	keyState.keycodes[KEY_SCROLLLOCK] = InputKeyCode::SCROLL_LOCK;
	keyState.keycodes[KEY_PAUSE] = InputKeyCode::PAUSE;
	keyState.keycodes[KEY_DELETE] = InputKeyCode::DELETE;
	keyState.keycodes[KEY_BACKSPACE] = InputKeyCode::BACKSPACE;
	keyState.keycodes[KEY_ENTER] = InputKeyCode::ENTER;
	keyState.keycodes[KEY_HOME] = InputKeyCode::HOME;
	keyState.keycodes[KEY_END] = InputKeyCode::END;
	keyState.keycodes[KEY_PAGEUP] = InputKeyCode::PAGE_UP;
	keyState.keycodes[KEY_PAGEDOWN] = InputKeyCode::PAGE_DOWN;
	keyState.keycodes[KEY_INSERT] = InputKeyCode::INSERT;
	keyState.keycodes[KEY_LEFT] = InputKeyCode::LEFT;
	keyState.keycodes[KEY_RIGHT] = InputKeyCode::RIGHT;
	keyState.keycodes[KEY_DOWN] = InputKeyCode::DOWN;
	keyState.keycodes[KEY_UP] = InputKeyCode::UP;
	keyState.keycodes[KEY_F1] = InputKeyCode::F1;
	keyState.keycodes[KEY_F2] = InputKeyCode::F2;
	keyState.keycodes[KEY_F3] = InputKeyCode::F3;
	keyState.keycodes[KEY_F4] = InputKeyCode::F4;
	keyState.keycodes[KEY_F5] = InputKeyCode::F5;
	keyState.keycodes[KEY_F6] = InputKeyCode::F6;
	keyState.keycodes[KEY_F7] = InputKeyCode::F7;
	keyState.keycodes[KEY_F8] = InputKeyCode::F8;
	keyState.keycodes[KEY_F9] = InputKeyCode::F9;
	keyState.keycodes[KEY_F10] = InputKeyCode::F10;
	keyState.keycodes[KEY_F11] = InputKeyCode::F11;
	keyState.keycodes[KEY_F12] = InputKeyCode::F12;
	keyState.keycodes[KEY_F13] = InputKeyCode::F13;
	keyState.keycodes[KEY_F14] = InputKeyCode::F14;
	keyState.keycodes[KEY_F15] = InputKeyCode::F15;
	keyState.keycodes[KEY_F16] = InputKeyCode::F16;
	keyState.keycodes[KEY_F17] = InputKeyCode::F17;
	keyState.keycodes[KEY_F18] = InputKeyCode::F18;
	keyState.keycodes[KEY_F19] = InputKeyCode::F19;
	keyState.keycodes[KEY_F20] = InputKeyCode::F20;
	keyState.keycodes[KEY_F21] = InputKeyCode::F21;
	keyState.keycodes[KEY_F22] = InputKeyCode::F22;
	keyState.keycodes[KEY_F23] = InputKeyCode::F23;
	keyState.keycodes[KEY_F24] = InputKeyCode::F24;
	keyState.keycodes[KEY_KPSLASH] = InputKeyCode::KP_DIVIDE;
	keyState.keycodes[KEY_KPASTERISK] = InputKeyCode::KP_MULTIPLY;
	keyState.keycodes[KEY_KPMINUS] = InputKeyCode::KP_SUBTRACT;
	keyState.keycodes[KEY_KPPLUS] = InputKeyCode::KP_ADD;
	keyState.keycodes[KEY_KP0] = InputKeyCode::KP_0;
	keyState.keycodes[KEY_KP1] = InputKeyCode::KP_1;
	keyState.keycodes[KEY_KP2] = InputKeyCode::KP_2;
	keyState.keycodes[KEY_KP3] = InputKeyCode::KP_3;
	keyState.keycodes[KEY_KP4] = InputKeyCode::KP_4;
	keyState.keycodes[KEY_KP5] = InputKeyCode::KP_5;
	keyState.keycodes[KEY_KP6] = InputKeyCode::KP_6;
	keyState.keycodes[KEY_KP7] = InputKeyCode::KP_7;
	keyState.keycodes[KEY_KP8] = InputKeyCode::KP_8;
	keyState.keycodes[KEY_KP9] = InputKeyCode::KP_9;
	keyState.keycodes[KEY_KPDOT] = InputKeyCode::KP_DECIMAL;
	keyState.keycodes[KEY_KPEQUAL] = InputKeyCode::KP_EQUAL;
	keyState.keycodes[KEY_KPENTER] = InputKeyCode::KP_ENTER;
	keyState.keycodes[KEY_102ND] = InputKeyCode::WORLD_2;

	return true;
}

static struct wl_surface_listener cursor_surface_listener = {
	[] (void *data, wl_surface *surface, wl_output *output) {
		auto seat = (WaylandSeat *)data;
		if (!WaylandLibrary::ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.emplace(out);
		seat->tryUpdateCursor();
	},
	[] (void *data, struct wl_surface *wl_surface, struct wl_output *output) {
		auto seat = (WaylandSeat *)data;
		if (!WaylandLibrary::ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.erase(out);
	}
};

void WaylandSeat::initCursors() {
	char *name = nullptr;
	int size = 24;

	get_cursor_settings(&name, &size);

	size *= pointerScale;

	if (!cursorTheme || cursorTheme->cursorSize != size || cursorTheme->cursorName != String(name)) {
		cursorTheme = Rc<WaylandCursorTheme>::create(root, name, size);
	}

	if (name) {
		//free(name);
	}

	if (!cursorSurface) {
		cursorSurface = wayland->wl_compositor_create_surface(root->compositor);
		wayland->wl_surface_add_listener(cursorSurface, &cursor_surface_listener, this);
	}
}

void WaylandSeat::tryUpdateCursor() {
	int32_t scale = 1;

	for (auto &it : pointerOutputs) {
		scale = std::max(scale, it->scale);
	}

	if (scale != pointerScale) {
		pointerScale = scale;
		initCursors();
		if (cursorTheme) {
			cursorTheme->setCursor(this);
		}
	}
}

void WaylandSeat::update() {
	if (!root->seatDirty) {
		return;
	}

	root->seatDirty = false;

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && !pointer) {
		pointer = wayland->wl_seat_get_pointer(seat);
		wayland->wl_pointer_add_listener(pointer, &s_WaylandPointerListener, this);
		pointerScale = 1;
		initCursors();
	} else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0 && pointer) {
		wayland->wl_pointer_release(pointer);
		pointer = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && !keyboard) {
		keyboard = wayland->wl_seat_get_keyboard(seat);
		wayland->wl_keyboard_add_listener(keyboard, &s_WaylandKeyboardListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0 && keyboard) {
		wayland->wl_keyboard_release(keyboard);
		keyboard = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) != 0 && !touch) {
		touch = wayland->wl_seat_get_touch(seat);
		wayland->wl_touch_add_listener(touch, &s_WaylandTouchListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) == 0 && touch) {
		wayland->wl_touch_release(touch);
		touch = NULL;
	}

	// wayland->wl_display_roundtrip(root->display);
}

InputKeyCode WaylandSeat::translateKey(uint32_t scancode) const {
	if (scancode < sizeof(keyState.keycodes) / sizeof(keyState.keycodes[0])) {
		return keyState.keycodes[scancode];
	}

	return InputKeyCode::Unknown;
}

xkb_keysym_t WaylandSeat::composeSymbol(xkb_keysym_t sym) const {
	if (sym == XKB_KEY_NoSymbol || !compose) {
		return sym;
	}
	if (root->xkb->xkb_compose_state_feed(compose, sym) != XKB_COMPOSE_FEED_ACCEPTED) {
		return sym;
	}
	switch (root->xkb->xkb_compose_state_get_status(compose)) {
	case XKB_COMPOSE_COMPOSED:
		return root->xkb->xkb_compose_state_get_one_sym(compose);
	case XKB_COMPOSE_COMPOSING:
	case XKB_COMPOSE_CANCELLED:
		return XKB_KEY_NoSymbol;
	case XKB_COMPOSE_NOTHING:
	default:
		return sym;
	}
}

WaylandDecoration::~WaylandDecoration() {
	if (viewport) {
		wayland->wp_viewport_destroy(viewport);
		viewport = nullptr;
	}
	if (subsurface) {
		wayland->wl_subsurface_destroy(subsurface);
		subsurface = nullptr;
	}
	if (surface) {
		display->destroyDecorationSurface(surface);
		surface = nullptr;
	}
}

bool WaylandDecoration::init(WaylandView *view, Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a, WaylandDecorationName n) {
	root = view;
	display = view->getDisplay();
	wayland = display->wayland;
	surface = display->createDecorationSurface(this);
	name = n;
	switch (n) {
	case WaylandDecorationName::RightSide: image = WaylandCursorImage::RightSide; break;
	case WaylandDecorationName::TopRigntCorner: image = WaylandCursorImage::TopRigntCorner; break;
	case WaylandDecorationName::TopSide: image = WaylandCursorImage::TopSide; break;
	case WaylandDecorationName::TopLeftCorner: image = WaylandCursorImage::TopLeftCorner; break;
	case WaylandDecorationName::BottomRightCorner: image = WaylandCursorImage::BottomRightCorner; break;
	case WaylandDecorationName::BottomSide: image = WaylandCursorImage::BottomSide; break;
	case WaylandDecorationName::BottomLeftCorner: image = WaylandCursorImage::BottomLeftCorner; break;
	case WaylandDecorationName::LeftSide: image = WaylandCursorImage::LeftSide; break;
	default:
		image = WaylandCursorImage::LeftPtr;
		break;
	}
	buffer = move(b);
	if (a) {
		active = move(a);
	}

	auto parent = root->getSurface();

	subsurface = wayland->wl_subcompositor_get_subsurface(display->subcompositor, surface, parent);
	wayland->wl_subsurface_place_below(subsurface, parent);
	wayland->wl_subsurface_set_sync(subsurface);

	viewport = wayland->wp_viewporter_get_viewport(display->viewporter, surface);
	wayland->wl_surface_attach(surface, buffer->buffer, 0, 0);

	dirty = true;

	return true;
}

bool WaylandDecoration::init(WaylandView *view, Rc<WaylandBuffer> &&b, WaylandDecorationName n) {
	return init(view, move(b), nullptr, n);
}

void WaylandDecoration::setAltBuffers(Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a) {
	altBuffer = move(b);
	altActive = move(a);
}

void WaylandDecoration::handlePress(uint32_t s, uint32_t button, uint32_t state) {
	serial = s;
	waitForMove = false;
	if (isTouchable()) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial);
		}
	} else if (image == WaylandCursorImage::LeftPtr) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			auto n = Time::now().toMicros();
			if (n - lastTouch < 500'000) {
				root->handleDecorationPress(this, serial, true);
			}
			lastTouch = n;
		} else if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			waitForMove = true;
			//root->handleDecorationPress(this, serial);
		}
	} else {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial);
		}
	}
}

void WaylandDecoration::handleMotion() {
	if (waitForMove) {
		root->handleDecorationPress(this, serial);
	}
}

void WaylandDecoration::onEnter() {
	if (isTouchable() && !isActive) {
		isActive = true;
		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::onLeave() {
	if (isTouchable() && isActive) {
		isActive = false;
		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setActive(bool val) {
	if (!isTouchable()) {
		if (val != isActive) {
			isActive = val;
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
			dirty = true;
		}
	}
}

void WaylandDecoration::setVisible(bool val) {
	if (val != visible) {
		visible = val;
		if (visible) {
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		} else {
			wayland->wl_surface_attach(surface, nullptr, 0, 0);
		}
		dirty = true;
	}
}

void WaylandDecoration::setAlternative(bool val) {
	if (!altBuffer || !altActive) {
		return;
	}

	if (alternative != val) {
		alternative = val;
		auto tmpA = move(altBuffer);
		auto tmpB = move(altActive);

		altBuffer = move(buffer);
		altActive = move(active);

		buffer = move(tmpA);
		active = move(tmpB);

		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setGeometry(int32_t x, int32_t y, int32_t width, int32_t height) {
	if (_x == x && _y == y && _width == width && _height == height) {
		return;
	}

	_x = x;
	_y = y;
	_width = width;
	_height = height;

	wayland->wl_subsurface_set_position(subsurface, _x,_y);
	wayland->wp_viewport_set_destination(viewport, _width, _height);

	auto &b = (active && isActive) ? active : buffer;
	wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
	dirty = true;
}

bool WaylandDecoration::commit() {
	if (dirty) {
		wayland->wl_surface_commit(surface);
		dirty = false;
		return true;
	}
	return false;
}

bool WaylandDecoration::isTouchable() const {
	switch (name) {
	case WaylandDecorationName::IconClose:
	case WaylandDecorationName::IconMaximize:
	case WaylandDecorationName::IconMinimize:
	case WaylandDecorationName::IconRestore:
		return true;
		break;
	default:
		break;
	}
	return false;
}

}

#endif
