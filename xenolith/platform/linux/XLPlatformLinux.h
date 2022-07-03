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
	enum xkb_state_component (* xkb_state_update_mask) (struct xkb_state *, xkb_mod_mask_t depressed_mods,
			xkb_mod_mask_t latched_mods, xkb_mod_mask_t locked_mods, xkb_layout_index_t depressed_layout,
			xkb_layout_index_t latched_layout, xkb_layout_index_t locked_layout) = nullptr;
	int (* xkb_state_key_get_utf8) (struct xkb_state *state, xkb_keycode_t key, char *buffer, size_t size) = nullptr;
	uint32_t (* xkb_state_key_get_utf32) (struct xkb_state *state, xkb_keycode_t key) = nullptr;
	xkb_keysym_t (* xkb_state_key_get_one_sym) (struct xkb_state *state, xkb_keycode_t key) = nullptr;
	void (* xkb_keymap_key_for_each) (struct xkb_keymap *keymap, xkb_keymap_key_iter_t iter, void *data) = nullptr;
	const char * (* xkb_keymap_key_get_name) (struct xkb_keymap *keymap, xkb_keycode_t key) = nullptr;

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
	virtual VkSurfaceKHR createWindowSurface(vk::Instance *instance) const = 0;

	virtual int getSocketFd() const = 0;

	virtual bool poll() = 0;
	virtual uint64_t getScreenFrameInterval() const = 0;

	virtual void mapWindow() = 0;

	virtual void onSurfaceInfo(gl::SurfaceInfo &) const { }
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
	virtual void updateTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	bool isInputEnabled() const { return _inputEnabled; }
	LinuxViewInterface *getView() const { return _view; }

	vk::Device *getDevice() const { return _device; }

	// minimal poll interval
	virtual uint64_t getUpdateInterval() const override { return 1000; }

	virtual void mapWindow() override;

protected:
	virtual bool pollInput() override;

	virtual gl::SurfaceInfo getSurfaceOptions() const override;

	Rc<LinuxViewInterface> _view;
	URect _rect;
	String _name;
	int _eventFd = -1;
	bool _inputEnabled = false;
};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUX_H_ */
