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

#include "XLGlView.h"
#include "XLGlLoop.h"
#include "XLScene.h"

namespace stappler::xenolith::gl {

XL_DECLARE_EVENT_CLASS(View, onClipboard);
XL_DECLARE_EVENT_CLASS(View, onBackground);
XL_DECLARE_EVENT_CLASS(View, onFocus);
XL_DECLARE_EVENT_CLASS(View, onScreenSize);

View::View() { }

View::~View() { }

bool View::init(const Rc<EventLoop> &ev, const Rc<gl::Loop> &loop) {
	if (!FrameEmitter::init(loop, 1'000'000 / 60)) {
		return false;
	}

	_eventLoop = ev;
	return true;
}

bool View::begin(const Rc<Director> &dir, Function<void()> &&cb) {
	_director = dir;
	_onEnded = move(cb);
	_eventLoop->addView(this);
	_director->begin(this);
	_director->update();
	return true;
}

void View::end() {
	_loop->performOnThread([this] {
		invalidate();
	}, this);

	if (_onEnded) {
		_onEnded();
		_onEnded = nullptr;
	}
	_director->end();
	_director = nullptr;
	_eventLoop->removeView(this);
}

void View::reset(SwapchanCreationMode mode) {
	if (_swapchain) {
		_swapchain->recreateSwapchain(*_loop->getDevice(), mode);
	}
}

void View::update() {
	if (_director) {
		_director->update();
	}
}

int View::getDpi() const {
	return _dpi;
}

float View::getDensity() const {
	return _density;
}

const Extent2 & View::getScreenExtent() const {
	return _screenExtent;
}

void View::setScreenExtent(Extent2 e) {
	if (e != _screenExtent) {
		_screenExtent = e;
		onScreenSize(this);
		if (_loop) {
			dropFrameTimeout();
		}
	}
}

void View::handleTouchesBegin(int num, intptr_t ids[], float xs[], float ys[]) { }

void View::handleTouchesMove(int num, intptr_t ids[], float xs[], float ys[]) { }

void View::handleTouchesEnd(int num, intptr_t ids[], float xs[], float ys[]) { }

void View::handleTouchesCancel(int num, intptr_t ids[], float xs[], float ys[]) { }

void View::setClipboardString(StringView) { }

StringView View::getClipboardString() const { return StringView(); }

ScreenOrientation View::getScreenOrientation() const {
	return _orientation;
}

bool View::isTouchDevice() const {
	return _isTouchDevice;
}

bool View::hasFocus() const {
	return _hasFocus;
}

bool View::isInBackground() const {
	return _inBackground;
}

void View::pushEvent(AppEvent::Value events) const {
	_events |= events;
}

AppEvent::Value View::popEvents() const {
	return _events.exchange(AppEvent::None);
}

void View::acquireNextFrame() {
	pushEvent(AppEvent::Update);
}

void View::runFrame(const Rc<gl::RenderQueue> &queue, Extent2 extent) {
	auto req = Rc<FrameRequest>::create(queue, this, extent);
	req->bindSwapchain(_swapchain);
	submitNextFrame(move(req));
}

gl::SwapchainConfig View::selectConfig(const gl::SurfaceInfo &info) {
	auto ret = _director->selectConfig(info);
	setScreenExtent(ret.extent);
	return ret;
}

}
