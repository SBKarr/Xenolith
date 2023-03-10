/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "MaterialInputField.h"
#include "XLDirector.h"
#include "XLLayer.h"

namespace stappler::xenolith::material {

InputField::~InputField() { }

bool InputField::init(InputFieldStyle fieldStyle) {
	SurfaceStyle style;
	switch (fieldStyle) {
	case InputFieldStyle::Filled:
		style.nodeStyle = NodeStyle::Filled;
		style.colorRole = ColorRole::SurfaceVariant;
		break;
	case InputFieldStyle::Outlined:
		style.nodeStyle = NodeStyle::Outlined;
		style.shapeStyle = ShapeStyle::ExtraSmall;
		break;
	}

	return init(fieldStyle, style);
}

bool InputField::init(InputFieldStyle fieldStyle, const SurfaceStyle &surfaceStyle) {
	if (!Surface::init(surfaceStyle)) {
		return false;
	}

	_container = addChild(Rc<InputTextContainer>::create(), ZOrder(1));
	_container->setAnchorPoint(Anchor::BottomLeft);

	_labelText = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodyLarge), ZOrder(1));
	_labelText->setAnchorPoint(Anchor::MiddleLeft);

	_supportingText = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodySmall), ZOrder(1));
	_supportingText->setAnchorPoint(Anchor::TopLeft);

	_leadingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_leadingIcon->setContentSize(Size2(24.0f, 24.0f));

	_trailingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_trailingIcon->setAnchorPoint(Anchor::MiddleRight);
	_trailingIcon->setContentSize(Size2(24.0f, 24.0f));

	_indicator = addChild(Rc<Surface>::create(SurfaceStyle(ColorRole::OnSurfaceVariant, NodeStyle::Filled)), ZOrder(1));
	_indicator->setAnchorPoint(Anchor::BottomLeft);

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->addMouseOverRecognizer([this] (const GestureData &data) {
		_mouseOver = (data.event == GestureEvent::Began);
		updateActivityState();
		return true;
	});
	_inputListener->addTapRecognizer([this] (const GestureTap &tap) {
		if (!_focused) {
			acquireInput(tap.input->currentLocation);
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	_focusInputListener = addInputListener(Rc<InputListener>::create());
	_focusInputListener->setPriority(1);
	_focusInputListener->addTapRecognizer([this] (const GestureTap &tap) {
		if (_handler.isActive()) {
			_handler.cancel();
		}
		_focusInputListener->setEnabled(false);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);
	_focusInputListener->setTouchFilter([this] (const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (!_container->isTouched(event.currentLocation, 8.0f)) {
			return true;
		}
		return false;
	});
	_focusInputListener->setEnabled(false);

	_handler.onText = std::bind(&InputField::handleTextInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_handler.onKeyboard = std::bind(&InputField::handleKeyboardEnabled, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_handler.onInput = std::bind(&InputField::handleInputEnabled, this, std::placeholders::_1);

	return true;
}

void InputField::onContentSizeDirty() {
	Surface::onContentSizeDirty();

	_supportingText->setPosition(Vec2(16.0f, -4.0f));
	_supportingText->setWidth(_contentSize.width - 32.0f);

	_leadingIcon->setPosition(Vec2(12.0f, _contentSize.height / 2.0f));
	_trailingIcon->setPosition(Vec2(_contentSize.width - 12.0f, _contentSize.height / 2.0f));

	float xOffset = 16.0f;
	float containerWidth = _contentSize.width - 16.0f * 2.0f;

	if (getLeadingIconName() != IconName::None) {
		xOffset += _leadingIcon->getContentSize().width + 12.0f;
		containerWidth -= _leadingIcon->getContentSize().width + 12.0f;
	}

	if (getTrailingIconName() != IconName::None) {
		containerWidth -= _trailingIcon->getContentSize().width + 12.0f;
	}

	_container->setContentSize(Size2(containerWidth, _contentSize.height - 32.0f));
	_container->setPosition(Vec2(xOffset, 10.0f));

	if (_focused) {
		_labelText->setAnchorPoint(Anchor::TopLeft);
		_labelText->setPosition(Vec2(xOffset, _contentSize.height - 9.0f));
		_indicator->setContentSize(Size2(_contentSize.width, 2.0f));
	} else {
		if (!_inputString.empty()) {
			_labelText->setAnchorPoint(Anchor::TopLeft);
			_labelText->setPosition(Vec2(xOffset, _contentSize.height - 9.0f));
		} else {
			_labelText->setAnchorPoint(Anchor::MiddleLeft);
			_labelText->setPosition(Vec2(xOffset, _contentSize.height / 2.0f));
		}
		_indicator->setContentSize(Size2(_contentSize.width, 1.0f));
	}

	stopAllActionsByTag(InputEnabledActionTag);
}

void InputField::setLabelText(StringView text) {
	_labelText->setString(text);
}

StringView InputField::getLabelText() const {
	return _labelText->getString8();
}

void InputField::setSupportingText(StringView text) {
	_supportingText->setString(text);
}

StringView InputField::getSupportingText() const {
	return _supportingText->getString8();
}

void InputField::setLeadingIconName(IconName name) {
	if (name != getLeadingIconName()) {
		_leadingIcon->setIconName(name);
		_contentSizeDirty = true;
	}
}

IconName InputField::getLeadingIconName() const {
	return _leadingIcon->getIconName();
}

void InputField::setTrailingIconName(IconName name) {
	if (name != getTrailingIconName()) {
		_trailingIcon->setIconName(name);
		_contentSizeDirty = true;
	}
}

IconName InputField::getTrailingIconName() const {
	return _trailingIcon->getIconName();
}

void InputField::updateActivityState() {
	auto style = getStyleTarget();
	if (!_enabled) {
		style.activityState = ActivityState::Disabled;
	} else if (_focused) {
		style.activityState = ActivityState::Enabled;
	} else if (_mouseOver) {
		style.activityState = ActivityState::Hovered;
	} else {
		style.activityState = ActivityState::Enabled;
	}
	setStyle(style, _activityAnimationDuration);
}

void InputField::updateInputEnabled() {
	if (!_running) {
		_contentSizeDirty = true;
		return;
	}

	stopAllActionsByTag(InputEnabledActionTag);

	bool populated = !_inputString.empty();

	auto labelAnchor = _labelText->getAnchorPoint();
	auto labelPos = _labelText->getPosition();
	auto indicatorSize = _indicator->getContentSize();

	auto targetLabelAnchor = Anchor::MiddleLeft;
	auto targetLabelPos = Vec3(labelPos.x, _contentSize.height / 2.0f, 0.0f);
	auto targetIndicatorSize = Size2(indicatorSize.width, _focused ? 2.0f : 1.0f);

	auto sourceLabelSize = _labelText->getFontSize();
	auto targetLabelSize = Label::FontSize(16);

	auto sourceBlendValue = _labelText->getBlendColorValue();
	auto targetBlendValue = _focused ? 1.0f : 0.0f;

	if (populated || _focused) {
		targetLabelAnchor = Anchor::TopLeft;
		targetLabelPos = Vec3(labelPos.x, _contentSize.height - 9.0f, 0.0f);
		targetLabelSize = Label::FontSize(12);
	}

	runAction(makeEasing(Rc<ActionProgress>::create(_activityAnimationDuration, [=, this] (float p) {
		_labelText->setAnchorPoint(progress(labelAnchor, targetLabelAnchor, p));
		_labelText->setPosition(progress(labelPos, targetLabelPos, p));
		_labelText->setFontSize(progress(sourceLabelSize, targetLabelSize, p));
		_indicator->setContentSize(progress(indicatorSize, targetIndicatorSize, p));
	}), EasingType::Standard), InputEnabledActionTag);

	runAction(makeEasing(Rc<ActionProgress>::create(_activityAnimationDuration, [=, this] (float p) {
		_labelText->setBlendColor(ColorRole::Primary, progress(sourceBlendValue, targetBlendValue, p));
	}), EasingType::Standard), InputEnabledLabelActionTag);

	auto indicatorStyle = _indicator->getStyleTarget();
	indicatorStyle.colorRole = _focused ? ColorRole::Primary : ColorRole::OnSurfaceVariant;

	_indicator->setStyle(indicatorStyle, _activityAnimationDuration);
}

void InputField::acquireInput(const Vec2 &targetLocation) {
	_cursor = TextCursor(_inputString.size(), 0);
	_markedRegion = TextCursor::InvalidCursor;
	_handler.run(_director->getTextInputManager(), _inputString, _cursor, _markedRegion, _inputType);
	_focusInputListener->setEnabled(true);
}

void InputField::handleTextInput(WideStringView str, TextCursor cursor, TextCursor marked) {
	auto label = _container->getLabel();

	auto maxChars = label->getMaxChars();
	if (maxChars > 0) {
		if (maxChars < str.size()) {
			auto tmpString = WideStringView(str, 0, maxChars);
			_handler.setString(tmpString, cursor);
			handleTextInput(tmpString, _cursor, _markedRegion);
			handleError(InputFieldError::Overflow);
			return;
		}
	}

	for (auto &it : str) {
		if (!handleInputChar(it)) {
			_handler.setString(getInputString(), _cursor);
			handleError(InputFieldError::Overflow);
			return;
		}
	}

	//bool isInsert = str.size() > _inputString.size();

	_container->setCursor(cursor);

	_inputString = str.str<Interface>();
	_cursor = cursor;

	switch (_passwordMode) {
	case InputFieldPasswordMode::NotPassword:
	case InputFieldPasswordMode::ShowAll:
		label->setString(_inputString);
		break;
	case InputFieldPasswordMode::ShowNone:
	case InputFieldPasswordMode::ShowChar: {
		WideString str; str.resize(_inputString.length(), u'*');
		label->setString(str);
		/*if (isInsert) {
			showLastChar();
		}*/
		break;
	}
	}

	label->tryUpdateLabel();

	_container->handleLabelChanged();
}

void InputField::handleKeyboardEnabled(bool, const Rect &, float) {

}

void InputField::handleInputEnabled(bool enabled) {
	if (_focused != enabled) {
		_focused = enabled;
		updateActivityState();
		updateInputEnabled();
	}
	_container->setEnabled(enabled);
}

bool InputField::handleInputChar(char16_t ch) {
	return true;
}

void InputField::handleError(InputFieldError err) {

}

}
