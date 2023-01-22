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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_

#include "XLPlatform.h"

#if LINUX

#include "XLVkView.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-names.h>

#define XL_X11_DEBUG 1
#define XL_WAYLAND_DEBUG 0

#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::vtext("X11", __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif

#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::vtext("Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif

namespace stappler::xenolith::platform {

enum class SurfaceType : uint32_t {
	None,
	XCB = 1 << 0,
	Wayland = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(SurfaceType)

static constexpr uint32_t INVALID_CODEPOINT = 0xffffffffu;

/* We use GLFW mapping table for keysyms */
SP_EXTERN_C uint32_t _glfwKeySym2Unicode(unsigned int keysym);

class XkbLibrary : public Ref {
public:
	static XkbLibrary *getInstance();

	XkbLibrary() { }

	virtual ~XkbLibrary();

	bool init();
	void close();

	bool hasX11() const { return _x11; }

	struct xkb_context *getContext() const { return _context; }

	struct xkb_context * (* xkb_context_new) (enum xkb_context_flags flags) = nullptr;
	struct xkb_context * (* xkb_context_ref) (struct xkb_context *context) = nullptr;
	void (* xkb_context_unref)  (struct xkb_context *context) = nullptr;
	void (* xkb_keymap_unref) (struct xkb_keymap *keymap) = nullptr;
	void (* xkb_state_unref) (struct xkb_state *state) = nullptr;
	struct xkb_keymap* (*xkb_keymap_new_from_string)(struct xkb_context *context, const char *string,
			enum xkb_keymap_format format, enum xkb_keymap_compile_flags flags) = nullptr;
	struct xkb_state * (* xkb_state_new) (struct xkb_keymap *keymap) = nullptr;
	enum xkb_state_component (* xkb_state_update_mask) (struct xkb_state *, xkb_mod_mask_t depressed_mods,
			xkb_mod_mask_t latched_mods, xkb_mod_mask_t locked_mods, xkb_layout_index_t depressed_layout,
			xkb_layout_index_t latched_layout, xkb_layout_index_t locked_layout) = nullptr;
	int (* xkb_state_key_get_utf8) (struct xkb_state *state, xkb_keycode_t key, char *buffer, size_t size) = nullptr;
	uint32_t (* xkb_state_key_get_utf32) (struct xkb_state *state, xkb_keycode_t key) = nullptr;
	xkb_keysym_t (* xkb_state_key_get_one_sym) (struct xkb_state *state, xkb_keycode_t key) = nullptr;
	int (* xkb_state_mod_index_is_active) (struct xkb_state *state, xkb_mod_index_t idx, enum xkb_state_component type) = nullptr;
	int (* xkb_state_key_get_syms)(struct xkb_state *state, xkb_keycode_t key, const xkb_keysym_t **syms_out) = nullptr;
	struct xkb_keymap * (* xkb_state_get_keymap) (struct xkb_state *state) = nullptr;
	void (* xkb_keymap_key_for_each) (struct xkb_keymap *keymap, xkb_keymap_key_iter_t iter, void *data) = nullptr;
	const char * (* xkb_keymap_key_get_name) (struct xkb_keymap *keymap, xkb_keycode_t key) = nullptr;
	xkb_mod_index_t (* xkb_keymap_mod_get_index) (struct xkb_keymap *keymap, const char *name) = nullptr;
	int (* xkb_keymap_key_repeats) (struct xkb_keymap *keymap, xkb_keycode_t key) = nullptr;
	uint32_t (* xkb_keysym_to_utf32) (xkb_keysym_t keysym) = nullptr;

	struct xkb_compose_table * (* xkb_compose_table_new_from_locale) (struct xkb_context *context,
			const char *locale, enum xkb_compose_compile_flags flags) = nullptr;
	void (* xkb_compose_table_unref) (struct xkb_compose_table *table) = nullptr;
	struct xkb_compose_state* (* xkb_compose_state_new)(struct xkb_compose_table *table, enum xkb_compose_state_flags flags) = nullptr;
	enum xkb_compose_feed_result (* xkb_compose_state_feed) (struct xkb_compose_state *state, xkb_keysym_t keysym) = nullptr;
	void (* xkb_compose_state_reset) (struct xkb_compose_state *state) = nullptr;
	enum xkb_compose_status (* xkb_compose_state_get_status) (struct xkb_compose_state *state) = nullptr;
	xkb_keysym_t (* xkb_compose_state_get_one_sym) (struct xkb_compose_state *state) = nullptr;
	void (* xkb_compose_state_unref) (struct xkb_compose_state *state) = nullptr;

	int (* xkb_x11_setup_xkb_extension) (xcb_connection_t *connection, uint16_t major_xkb_version, uint16_t minor_xkb_version,
			enum xkb_x11_setup_xkb_extension_flags flags, uint16_t *major_xkb_version_out, uint16_t *minor_xkb_version_out,
			uint8_t *base_event_out, uint8_t *base_error_out) = nullptr;
	int32_t (* xkb_x11_get_core_keyboard_device_id) (xcb_connection_t *connection) = nullptr;
	struct xkb_keymap * (* xkb_x11_keymap_new_from_device) (struct xkb_context *context,
			xcb_connection_t *connection, int32_t device_id, enum xkb_keymap_compile_flags flags) = nullptr;
	struct xkb_state * (* xkb_x11_state_new_from_device) (struct xkb_keymap *keymap,
			xcb_connection_t *connection, int32_t device_id) = nullptr;

protected:
	bool open(void *);
	void openAux();

	void *_handle = nullptr;
	void *_x11 = nullptr;
	struct xkb_context *_context = nullptr;
};

class LinuxViewInterface : public Ref {
public:
	virtual ~LinuxViewInterface() { }
	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance, VkPhysicalDevice dev) const = 0;

	virtual int getSocketFd() const = 0;

	virtual bool poll(bool frameReady) = 0;
	virtual uint64_t getScreenFrameInterval() const = 0;

	virtual void mapWindow() = 0;

	virtual void scheduleFrame() { }
	virtual void onSurfaceInfo(gl::SurfaceInfo &) const { }

	virtual void commit(uint32_t width, uint32_t height) { }
};

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void wakeup() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	virtual void presentWithQueue(vk::DeviceQueue &, Rc<ImageStorage> &&) override;

	bool isInputEnabled() const { return _inputEnabled; }
	LinuxViewInterface *getView() const { return _view; }

	vk::Device *getDevice() const { return _device; }

	// minimal poll interval
	virtual uint64_t getUpdateInterval() const override { return 1000; }

	virtual void mapWindow() override;

protected:
	virtual bool pollInput(bool frameReady) override;

	virtual gl::SurfaceInfo getSurfaceOptions() const override;

	virtual void finalize() override;

	Rc<LinuxViewInterface> _view;
	URect _rect;
	String _name;
	int _eventFd = -1;
	bool _inputEnabled = false;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_ */
