/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_COMMON_XLGLVIEW_H_
#define XENOLITH_GL_COMMON_XLGLVIEW_H_

#include "XLGl.h"
#include "XLGlFrameEmitter.h"
#include "XLGlSwapchain.h"

namespace stappler::xenolith {

class EventLoop;

}

namespace stappler::xenolith::gl {

class Device;
class Loop;
class Instance;
class Swapchain;

class View : public FrameEmitter {
public:
	static EventHeader onClipboard;
	static EventHeader onBackground;
	static EventHeader onFocus;
	static EventHeader onScreenSize;

	View();
	virtual ~View();

	virtual bool init(const Rc<EventLoop> &, const Rc<gl::Loop> &);

	virtual bool begin(const Rc<Director> &, Function<void()> &&completeCallback);
	virtual void end();

	virtual void setIMEKeyboardState(bool open) = 0;

	virtual void reset(SwapchanCreationMode mode);
	virtual void update();
	virtual bool poll() = 0; // poll for input
	virtual void close() = 0;

	virtual void setCursorVisible(bool isVisible) { }

	virtual int getDpi() const;
	virtual float getDensity() const;

	virtual const Rc<Director> &getDirector() const;

	virtual const Extent2 & getScreenExtent() const;
	virtual void setScreenExtent(Extent2);

	virtual void handleInputEvent(const InputEventData &);

	virtual void handleFocusIn();
	virtual void handleFocusOut();

	virtual void setClipboardString(StringView);
	virtual StringView getClipboardString() const;

	virtual ScreenOrientation getScreenOrientation() const;

	virtual bool isTouchDevice() const;
	virtual bool hasFocus() const;
	virtual bool isInBackground() const;

	// Push view event to process in in View's primary thread
	virtual void pushEvent(AppEvent::Value) const;
	virtual AppEvent::Value popEvents() const;

	virtual void runFrame(const Rc<gl::RenderQueue> &, Extent2);

	virtual gl::SwapchainConfig selectConfig(const gl::SurfaceInfo &);

protected:
	virtual void acquireNextFrame() override;

	Extent2 _screenExtent;

	int _dpi = 96;
	float _density = 1.0f;

	ScreenOrientation _orientation = ScreenOrientation::Landscape;

	bool _isTouchDevice = false;
	bool _inBackground = false;
	bool _hasFocus = true;

	mutable std::atomic<AppEvent::Value> _events = AppEvent::None;

	Function<void()> _onEnded;
	Rc<Director> _director;
	Rc<EventLoop> _eventLoop;
	Rc<Swapchain> _swapchain;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLVIEW_H_ */
