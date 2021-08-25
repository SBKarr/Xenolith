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

#ifndef XENOLITH_GL_COMMON_XLGLVIEW_H_
#define XENOLITH_GL_COMMON_XLGLVIEW_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

class Device;
class Loop;
class Instance;

namespace ViewEvent {
	using Value = uint32_t;

	constexpr uint32_t None = 0;
	constexpr uint32_t Terminate = 1;
	constexpr uint32_t SwapchainRecreation = 2;
	constexpr uint32_t SwapchainRecreationBest = 4;
	constexpr uint32_t Update = 8;
	constexpr uint32_t Thread = 16;
}

class View : public Ref {
public:
	static EventHeader onClipboard;
	static EventHeader onBackground;
	static EventHeader onFocus;
	static EventHeader onScreenSize;

	View();
	virtual ~View();

	virtual bool init(Instance *, Device *);

	virtual void end() = 0;

	virtual void setIMEKeyboardState(bool open) = 0;

	virtual bool run(Application *, Rc<Director>, const Callback<bool(uint64_t)> &) = 0;

	virtual void setCursorVisible(bool isVisible) { }

	virtual int getDpi() const;
	virtual float getDensity() const;

	virtual const Size & getScreenSize() const;
	virtual void setScreenSize(float width, float height);

	virtual void handleTouchesBegin(int num, intptr_t ids[], float xs[], float ys[]);
	virtual void handleTouchesMove(int num, intptr_t ids[], float xs[], float ys[]);
	virtual void handleTouchesEnd(int num, intptr_t ids[], float xs[], float ys[]);
	virtual void handleTouchesCancel(int num, intptr_t ids[], float xs[], float ys[]);

	virtual void enableOffscreenContext();
	virtual void disableOffscreenContext();

	virtual void setClipboardString(StringView);
	virtual StringView getClipboardString() const;

	virtual ScreenOrientation getScreenOrientation() const;

	virtual bool isTouchDevice() const;
	virtual bool hasFocus() const;
	virtual bool isInBackground() const;

	// Push view event to process in in View's primary thread
	virtual void pushEvent(ViewEvent::Value);
	virtual ViewEvent::Value popEvents();

	const Rc<Device> &getDevice() const;
	const Rc<Loop> &getLoop() const;

protected:
	Size _screenSize;

	int _dpi = 96;
	float _density = 1.0f;

	ScreenOrientation _orientation = ScreenOrientation::Landscape;

	bool _isTouchDevice = false;
	bool _inBackground = false;
	bool _hasFocus = true;

	std::atomic<ViewEvent::Value> _events = ViewEvent::None;
	Rc<Instance> _glInstance;
	Rc<Device> _glDevice; // logical presentation device
	Rc<Loop> _loop;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLVIEW_H_ */
