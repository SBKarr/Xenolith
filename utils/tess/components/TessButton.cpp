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

#include "TessButton.h"
#include "XLInputListener.h"
#include "XLAction.h"

namespace stappler::xenolith::tessapp {

bool Button::init(Function<void()> &&cb) {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_callback = move(cb);

	_listener = addInputListener(Rc<InputListener>::create());
	_listener->setTouchFilter([] (const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return true;
	});
	_listener->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		bool touched = isTouched(ev.currentLocation);
		if (touched != _focus) {
			_focus = touched;
			if (_focus) {
				handleFocusEnter();
			} else {
				handleFocusLeave();
			}
		}
		return true;
	});
	_listener->addTouchRecognizer([this] (GestureEvent event, const InputEvent &ev) -> bool {
		if (event == GestureEvent::Began) {
			if (isTouched(ev.currentLocation)) {
				_listener->setExclusive();
				return true;
			} else {
				return false;
			}
		} else if (event == GestureEvent::Ended) {
			if (isTouched(ev.currentLocation)) {
				handleTouch();
			}
		}
		return true;
	});
	_listener->setPointerEnterCallback([this] (bool pointerWithinWindow) {
		if (!pointerWithinWindow && _focus) {
			_focus = false;
			handleFocusLeave();
		}
		return true;
	});

	updateEnabled();

	return true;
}

void Button::setEnabled(bool value) {
	if (_enabled != value) {
		_enabled = value;
		updateEnabled();
	}
}

void Button::setCallback(Function<void()> &&cb) {
	_callback = cb;
}

void Button::handleFocusEnter() {
	stopAllActions();
	runAction(Rc<TintTo>::create(0.2f, Color::Red_200));
}

void Button::handleFocusLeave() {
	stopAllActions();
	runAction(Rc<TintTo>::create(0.2f, _enabled ? Color::Grey_400 : Color::Grey_200));
}

void Button::handleTouch() {
	_callback();
}

void Button::updateEnabled() {
	if (!_focus) {
		if (_running) {
			stopAllActions();
			runAction(Rc<TintTo>::create(0.2f, _enabled ? Color::Grey_400 : Color::Grey_200));
		} else {
			setColor(_enabled ? Color::Grey_400 : Color::Grey_200);
		}
	}
}


}
