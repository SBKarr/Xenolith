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

#include "MaterialButton.h"
#include "XLInputListener.h"

namespace stappler::xenolith::material {

static SurfaceStyle Button_getSurfaceStyle(NodeStyle style, ColorRole role, uint32_t schemeTag) {
	return SurfaceStyle(style, Elevation::Level1, ShapeStyle::Full, role, schemeTag);
}

bool Button::init(NodeStyle style, ColorRole role, uint32_t schemeTag) {
	return init(Button_getSurfaceStyle(style, role, schemeTag));
}

bool Button::init(const SurfaceStyle &style) {
	if (!Surface::init(style)) {
		return false;
	}

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::LabelLarge), ZOrder(1));
	_label->setAnchorPoint(Anchor::MiddleLeft);
	_label->setOnContentSizeDirtyCallback([this] {
		updateSizeFromContent();
	});

	_leadingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_leadingIcon->setContentSize(Size2(18.0f, 18.0f));

	_trailingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_trailingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_trailingIcon->setContentSize(Size2(18.0f, 18.0f));

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->addMouseOverRecognizer([this] (const GestureData &data) {
		_mouseOver = (data.event == GestureEvent::Began);
		updateActivityState();
		return true;
	});
	_inputListener->addPressRecognizer([this] (const GesturePress &press) {
		if (!_enabled || (_menuButtonListener->getSubscription() && !isMenuSourceButtonEnabled())) {
			return false;
		}

		if (press.event == GestureEvent::Began) {
			_longPressInit = false;
			_pressed = true;
			updateActivityState();
		} else if (press.event == GestureEvent::Activated) {
			_longPressInit = true;
		} else if (press.event == GestureEvent::Ended || press.event == GestureEvent::Cancelled) {
			_pressed = false;
			updateActivityState();
			if (press.event == GestureEvent::Ended) {
				_inputListener->setExclusiveForTouch(press.getId());
				if (_longPressInit) {
					handleLongPress();
				} else {
					handleTap();
				}
			}
		}
		return true;
	}, LongPressInterval);

	_inputListener->addTapRecognizer([this] (const GestureTap &tap) {
		if (!_enabled) {
			return false;
		}
		if (tap.count == 2) {
			_inputListener->setExclusiveForTouch(tap.getId());
			handleDoubleTap();
		}
		return true;
	});

	_menuButtonListener = addComponent(Rc<DataListener<MenuSourceButton>>::create([this] (SubscriptionFlags flags) {
		updateMenuButtonSource();
	}));

	return true;
}

void Button::onContentSizeDirty() {
	Surface::onContentSizeDirty();

	if (getLeadingIconName() != IconName::None && getTrailingIconName() == IconName::None && _label->getString().empty()) {
		_leadingIcon->setAnchorPoint(Anchor::Middle);
		_leadingIcon->setPosition(_contentSize / 2.0f);
	} else {
		_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);

		float contentWidth = getWidthForContent();
		float offset = (_contentSize.width - contentWidth) / 2.0f;

		Vec2 target(offset + (_styleTarget.nodeStyle == NodeStyle::Text ? 12.0f : 16.0f), _contentSize.height / 2.0f);

		if (getLeadingIconName() != IconName::None) {
			_leadingIcon->setPosition(target);
			target.x += 8.0f + _leadingIcon->getContentSize().width;
		} else {
			target.x += 8.0f;
		}

		_label->setPosition(target);
		target.x += _label->getContentSize().width + 8.0f;

		_trailingIcon->setPosition(target);
	}
}

void Button::setFollowContentSize(bool value) {
	if (value != _followContentSize) {
		_followContentSize = value;
		_contentSizeDirty = true;
		if (_followContentSize) {
			updateSizeFromContent();
		}
	}
}

bool Button::isFollowContentSize() const {
	return _followContentSize;
}

void Button::setSwallowEvents(bool value) {
	if (value) {
		_inputListener->setSwallowEvents(InputListener::EventMaskTouch);
	} else {
		_inputListener->clearSwallowEvents(InputListener::EventMaskTouch);
	}
}

bool Button::isSwallowEvents() const {
	return _inputListener->isSwallowAllEvents(InputListener::EventMaskTouch);
}

void Button::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		_inputListener->setEnabled(_enabled);
		updateActivityState();
	}
}

void Button::setSelected(bool val) {
	_selected = val;
	updateActivityState();
}

bool Button::isSelected() const {
	return _selected;
}

bool Button::isMenuSourceButtonEnabled() const {
	if (!_menuButtonListener) {
		return false;
	}

	return _menuButtonListener->getSubscription()->getCallback() != nullptr || _menuButtonListener->getSubscription()->getNextMenu();
}

void Button::setText(StringView text) {
	_label->setString(text);
}

StringView Button::getText() const {
	return _label->getString8();
}

void Button::setIconSize(float value) {
	if (value != getIconSize()) {
		_leadingIcon->setContentSize(Size2(value, value));
		_trailingIcon->setContentSize(Size2(value, value));
		updateSizeFromContent();
	}
}

float Button::getIconSize() const {
	return _leadingIcon->getContentSize().width;
}

void Button::setLeadingIconName(IconName name) {
	if (name != getLeadingIconName()) {
		_leadingIcon->setIconName(name);
		updateSizeFromContent();
	}
}

IconName Button::getLeadingIconName() const {
	return _leadingIcon->getIconName();
}

void Button::setTrailingIconName(IconName name) {
	if (name != getTrailingIconName()) {
		_trailingIcon->setIconName(name);
		updateSizeFromContent();
	}
}

IconName Button::getTrailingIconName() const {
	return _trailingIcon->getIconName();
}

void Button::setTapCallback(Function<void()> &&cb) {
	_callbackTap = move(cb);
}

void Button::setLongPressCallback(Function<void()> &&cb) {
	_callbackLongPress = move(cb);
}

void Button::setDoubleTapCallback(Function<void()> &&cb) {
	_callbackDoubleTap = move(cb);
}

void Button::setMenuSourceButton(Rc<MenuSourceButton> &&button) {
	if (button != _menuButtonListener->getSubscription()) {
		if (auto b = _menuButtonListener->getSubscription()) {
			b->onNodeDetached(this);
		}
		_menuButtonListener->setSubscription(button.get());
		updateMenuButtonSource();
		if (button) {
			button->onNodeAttached(this);
		}
	}
}

void Button::updateSizeFromContent() {
	if (!_followContentSize) {
		_contentSizeDirty = true;
		return;
	}

	Size2 targetSize;
	if (!_label->empty()) {
		targetSize = _label->getContentSize();
	} else {
		targetSize.height = getIconSize();
	}
	targetSize.width = getWidthForContent();
	targetSize.height += 24.0f;

	setContentSize(targetSize);
}

void Button::updateActivityState() {
	auto style = getStyleTarget();
	if (!_enabled || (_menuButtonListener->getSubscription() && !isMenuSourceButtonEnabled())) {
		style.activityState = ActivityState::Disabled;
	} else if (_pressed || _selected) {
		style.activityState = ActivityState::Pressed;
	} else if (_mouseOver) {
		style.activityState = ActivityState::Hovered;
	} else if (_focused) {
		style.activityState = ActivityState::Focused;
	} else {
		style.activityState = ActivityState::Enabled;
	}
	setStyle(style, _activityAnimationDuration);
}

void Button::handleTap() {
	if (auto btn = _menuButtonListener->getSubscription()) {
		auto &cb = btn->getCallback();
		if (cb) {
			cb(this, btn);
		}
		return;
	}
	if (_callbackTap) {
		auto id = retain();
		_callbackTap();
		release(id);
	}
}

void Button::handleLongPress() {
	if (_callbackLongPress) {
		auto id = retain();
		_callbackLongPress();
		release(id);
	}
}

void Button::handleDoubleTap() {
	if (_callbackDoubleTap) {
		auto id = retain();
		_callbackDoubleTap();
		release(id);
	}
}

float Button::getWidthForContent() const {
	float contentWidth = 0.0f;
	if (!_label->empty()) {
		contentWidth = ((_styleTarget.nodeStyle == NodeStyle::Text) ? 24.0f : 48.0f) + _label->getContentSize().width;
		if (_styleTarget.nodeStyle == NodeStyle::Text && (getLeadingIconName() != IconName::None || getTrailingIconName() != IconName::None)) {
			contentWidth += 16.0f;
		}
	} else {
		contentWidth = 24.0f;
	}

	if (getLeadingIconName() != IconName::None) {
		contentWidth += _leadingIcon->getContentSize().width;
	}
	if (getTrailingIconName() != IconName::None) {
		contentWidth += _trailingIcon->getContentSize().width;
	}
	return contentWidth;
}

void Button::updateMenuButtonSource() {
	if (auto btn = _menuButtonListener->getSubscription()) {
		_selected = btn->isSelected();
		_floatingMenuSource = btn->getNextMenu();

		setLeadingIconName(btn->getNameIcon());
		setTrailingIconName(btn->getValueIcon());
		setText(btn->getName());
	} else {
		_selected = false;
		_floatingMenuSource = nullptr;
	}
	updateActivityState();
}

}
