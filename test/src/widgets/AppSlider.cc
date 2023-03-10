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

#include "AppSlider.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool AppSlider::init(float value, Function<void(float)> &&cb) {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_value = value;
	_callback = move(cb);

	_foreground = addChild(Rc<Layer>::create(Color::Grey_500), ZOrder(1));
	_foreground->setPosition(Vec2::ZERO);
	_foreground->setAnchorPoint(Anchor::BottomLeft);

	_input = addInputListener(Rc<InputListener>::create());
	_input->addTouchRecognizer([this] (const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
		case GestureEvent::Activated:
			setValue(math::clamp(convertToNodeSpace(data.input->currentLocation).x / _contentSize.width, 0.0f, 1.0f));
			break;
		default:
			break;
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}));

	return true;
}

void AppSlider::onContentSizeDirty() {
	Layer::onContentSizeDirty();

	updateValue();
}

void AppSlider::setValue(float value) {
	if (_value != value) {
		_value = value;
		updateValue();
		if (_callback) {
			_callback(_value);
		}
	}
}

float AppSlider::getValue() const {
	return _value;
}

void AppSlider::setForegroundColor(const Color4F &color) {
	_foreground->setColor(color);
}

Color4F AppSlider::getForegroundColor() const {
	return _foreground->getColor();
}

void AppSlider::setBackgroundColor(const Color4F &color) {
	setColor(color);
}

Color4F AppSlider::getBackgroundColor() const {
	return getColor();
}

void AppSlider::updateValue() {
	_foreground->setContentSize(Size2(_contentSize.width * _value, _contentSize.height));
}


bool AppSliderWithLabel::init(StringView title, float value, Function<void(float)> &&cb) {
	if (!AppSlider::init(value, move(cb))) {
		return false;
	}

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(24);
	_label->setAnchorPoint(Anchor::MiddleLeft);
	_label->setString(title);

	_prefix = addChild(Rc<Label>::create());
	_prefix->setFontSize(24);
	_prefix->setAnchorPoint(Anchor::MiddleRight);
	_prefix->setAlignment(Label::Alignment::Right);

	return true;
}

void AppSliderWithLabel::onContentSizeDirty() {
	AppSlider::onContentSizeDirty();

	_label->setPosition(Vec2(_contentSize.width + 16.0f, _contentSize.height / 2.0f));
	_prefix->setPosition(Vec2(- 16.0f, _contentSize.height / 2.0f));
}

void AppSliderWithLabel::setString(StringView str) {
	_label->setString(str);
}

StringView AppSliderWithLabel::getString() const {
	return _label->getString8();
}

void AppSliderWithLabel::setPrefix(StringView str) {
	_prefix->setString(str);
}

StringView AppSliderWithLabel::getPrefix() const {
	return _prefix->getString8();
}

void AppSliderWithLabel::setFontSize(Label::FontSize size) {
	_label->setFontSize(size);
	_prefix->setFontSize(size);
}
void AppSliderWithLabel::setFontSize(uint16_t size) {
	_label->setFontSize(size);
	_prefix->setFontSize(size);
}

}
