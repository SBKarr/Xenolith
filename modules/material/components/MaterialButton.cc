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

bool Button::init(ButtonData &&data, NodeStyle style, ColorRole role, uint32_t schemeTag) {
	return init(move(data), Button_getSurfaceStyle(style, role, schemeTag));
}

bool Button::init(ButtonData &&data, const SurfaceStyle &style) {
	if (!Surface::init(style)) {
		return false;
	}

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::LabelLarge), 1);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setOnContentSizeDirtyCallback([this] {
		updateSizeFromContent();
	});

	_iconPrefix = addChild(Rc<VectorSprite>::create(Size2(24, 24)), 1);
	_iconPrefix->setAnchorPoint(Anchor::Middle);
	_iconPrefix->setVisible(false);

	_iconPostfix = addChild(Rc<VectorSprite>::create(Size2(24, 24)), 1);
	_iconPrefix->setAnchorPoint(Anchor::Middle);
	_iconPrefix->setVisible(false);

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->addMouseOverRecognizer([this] (const GestureData &data) {
		_mouseOver = (data.event == GestureEvent::Began);
		updateActivityState();
		return true;
	});
	_inputListener->addPressRecognizer([this] (const GesturePress &press) {
		if (!_enabled) {
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
			handleDoubleTap();
		}
		return true;
	});

	_buttonData = move(data);

	updateButtonData();

	return true;
}

void Button::onContentSizeDirty() {
	Surface::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

void Button::setFollowContentSize(bool value) {
	if (value != _buttonData.followContentSize) {
		_buttonData.followContentSize = value;
		_contentSizeDirty = true;
		if (_buttonData.followContentSize) {
			updateSizeFromContent();
		}
	}
}

bool Button::isFollowContentSize() const {
	return _buttonData.followContentSize;
}

void Button::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		updateActivityState();
	}
}

void Button::updateButtonData() {
	_label->setString(_buttonData.text);

	/*auto path = _iconPrefix->addPath();

	auto path = image->addPath();
	getIconData(iconName, [&] (BytesView bytes) {
		path->getPath()->init(bytes);
	});
	path->setWindingRule(vg::Winding::EvenOdd);
	path->setAntialiased(true);
	auto t = Mat4::IDENTITY;
	t.scale(1, -1, 1);
	t.translate(0, -24, 0);
	path->setTransform(t);*/

}

void Button::updateSizeFromContent() {
	if (!_buttonData.followContentSize) {
		return;
	}

	Size2 targetSize = _label->getContentSize();
	switch (_styleTarget.nodeStyle) {
	case NodeStyle::Text:
		targetSize.width += 12.0f * 2;
		break;
	default:
		targetSize.width += 24.0f * 2;
		break;
	}
	targetSize.height += 24.0f;

	setContentSize(targetSize);
}

void Button::updateActivityState() {
	auto style = getStyleTarget();
	if (!_enabled) {
		style.activityState = ActivityState::Disabled;
	} else if (_pressed) {
		style.activityState = ActivityState::Pressed;
	} else if (_mouseOver) {
		style.activityState = ActivityState::Hovered;
	} else if (_focused) {
		style.activityState = ActivityState::Focused;
	} else {
		style.activityState = ActivityState::Enabled;
	}
	setStyle(style, _buttonData.activityAnimationDuration);
}

void Button::handleTap() {
	if (_buttonData.callbackTap) {
		_buttonData.callbackTap();
	}
}

void Button::handleLongPress() {
	if (_buttonData.callbackLongPress) {
		_buttonData.callbackLongPress();
	}
}

void Button::handleDoubleTap() {
	if (_buttonData.callbackDoubleTap) {
		_buttonData.callbackDoubleTap();
	}
}

}
