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

#include "AppCheckbox.h"
#include "XLInputListener.h"
#include "XLLabel.h"

namespace stappler::xenolith::app {

bool AppCheckbox::init(bool value, Function<void(bool)> &&cb) {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_value = value;
	_callback = move(cb);

	setColor(_backgroundColor);
	setContentSize(Size2(32.0f, 32.0f));

	_input = addInputListener(Rc<InputListener>::create());
	_input->addTapRecognizer([this] (const GestureTap &data) {
		switch (data.event) {
		case GestureEvent::Activated:
			setValue(!_value);
			break;
		default:
			break;
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}));

	return true;
}

void AppCheckbox::setValue(bool value) {
	if (_value != value) {
		_value = value;
		updateValue();
		if (_callback) {
			_callback(_value);
		}
	}
}

bool AppCheckbox::getValue() const {
	return _value;
}

void AppCheckbox::setForegroundColor(const Color4F &color) {
	if (_foregroundColor != color) {
		_foregroundColor = color;
		updateValue();
	}
}

Color4F AppCheckbox::getForegroundColor() const {
	return _foregroundColor;
}

void AppCheckbox::setBackgroundColor(const Color4F &color) {
	if (_backgroundColor != color) {
		_backgroundColor = color;
		updateValue();
	}
}

Color4F AppCheckbox::getBackgroundColor() const {
	return _backgroundColor;
}

void AppCheckbox::updateValue() {
	if (_value) {
		setColor(_foregroundColor);
	} else {
		setColor(_backgroundColor);
	}
}

bool AppCheckboxWithLabel::init(StringView title, bool value, Function<void(bool)> &&cb) {
	if (!AppCheckbox::init(value, move(cb))) {
		return false;
	}

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(24);
	_label->setAnchorPoint(Anchor::MiddleLeft);
	_label->setString(title);

	return true;
}

void AppCheckboxWithLabel::onContentSizeDirty() {
	AppCheckbox::onContentSizeDirty();

	_label->setPosition(Vec2(_contentSize.width + 16.0f, _contentSize.height / 2.0f));
}

}
