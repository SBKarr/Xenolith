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

#include "AppInputKeyboardTest.h"
#include "XLLayer.h"
#include "XLDirector.h"
#include "XLApplication.h"
#include "XLInputDispatcher.h"

namespace stappler::xenolith::app {

class InputKeyboardOnScreenKey : public VectorSprite {
public:
	virtual ~InputKeyboardOnScreenKey() { }

	virtual bool init(Function<void(bool)> &&);

	void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

protected:
	using VectorSprite::init;

	Function<void(bool)> _callback;
	bool _enabled = false;
};

class InputKeyboardOnScreenKeyboard : public Node {
public:
	enum ActiveButton {
		Up,
		Down,
		Left,
		Right,
		None
	};

	using ActiveButtons = std::bitset<4>;

	virtual ~InputKeyboardOnScreenKeyboard() { }

	virtual bool init() override;
	virtual void onContentSizeDirty() override;

protected:
	bool handleTouchBegin(const InputEvent &);
	bool handleTouchMove(const InputEvent &);
	bool handleTouchEnd(const InputEvent &);

	ActiveButtons computeActiveButtons() const;
	void updateActiveButtons();

	ActiveButton getButtonForLocation(const Vec2 &) const;

	void sendInputEvent(ActiveButton, InputKeyCode, bool enabled);

	Vec2 _currentLocation;
	Map<uint32_t, Vec2> _touches;

	InputKeyboardOnScreenKey *_up = nullptr;
	InputKeyboardOnScreenKey *_right = nullptr;
	InputKeyboardOnScreenKey *_down = nullptr;
	InputKeyboardOnScreenKey *_left = nullptr;

	ActiveButtons _activeButtons;
};

bool InputKeyboardOnScreenKey::init(Function<void(bool)> &&cb) {
	auto image = Rc<VectorImage>::create(Size2(40, 50));
	// For solid RenderingLevel depth testing is enabled
	// so, if icon defined after background, it will be filtered by depth test

	// icon:
	image->addPath()->moveTo(10, 30).lineTo(20, 40).lineTo(30, 30).setStrokeWidth(4.0f)
			.setStyle(vg::DrawStyle::Stroke).setStrokeColor(Color::Black).setAntialiased(false);

	// background
	image->addPath()->moveTo(20, 0).lineTo(0, 20).lineTo(0, 50).lineTo(40, 50).lineTo(40, 20)
			.setFillColor(Color::Grey_200).setAntialiased(false);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	setContentSize(Size2(40, 50));

	_callback = move(cb);

	return true;
}

void InputKeyboardOnScreenKey::setEnabled(bool value) {
	if (_enabled != value) {
		_enabled = value;
		setColor(_enabled ? Color::Grey_500 : Color::White);
		if (_callback) {
			_callback(_enabled);
		}
	}
}

bool InputKeyboardOnScreenKeyboard::init() {
	if (!Node::init()) {
		return false;
	}

	_up = addChild(Rc<InputKeyboardOnScreenKey>::create([this] (bool value) {
		sendInputEvent(ActiveButton::Up, InputKeyCode::UP, value);
	}));
	_up->setAnchorPoint(Anchor::MiddleBottom - Vec2(0.0f, 0.1f));

	_right = addChild(Rc<InputKeyboardOnScreenKey>::create([this] (bool value) {
		sendInputEvent(ActiveButton::Right, InputKeyCode::RIGHT, value);
	}));
	_right->setRotation(90.0_to_rad);
	_right->setAnchorPoint(Anchor::MiddleBottom - Vec2(0.0f, 0.1f));

	_left = addChild(Rc<InputKeyboardOnScreenKey>::create([this] (bool value) {
		sendInputEvent(ActiveButton::Left, InputKeyCode::LEFT, value);
	}));
	_left->setRotation(-90.0_to_rad);
	_left->setAnchorPoint(Anchor::MiddleBottom - Vec2(0.0f, 0.1f));

	_down = addChild(Rc<InputKeyboardOnScreenKey>::create([this] (bool value) {
		sendInputEvent(ActiveButton::Down, InputKeyCode::DOWN, value);
	}));
	_down->setRotation(180.0_to_rad);
	_down->setAnchorPoint(Anchor::MiddleBottom - Vec2(0.0f, 0.1f));

	setContentSize(Size2(120.0f, 120.0f));

	auto l = addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (const GestureData &input) {
		_currentLocation = input.input->currentLocation;
		switch (input.event) {
		case GestureEvent::Began:
			return handleTouchBegin(*input.input);
			break;
		case GestureEvent::Activated:
			return handleTouchMove(*input.input);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			return handleTouchEnd(*input.input);
			break;
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}));

	return true;
}

void InputKeyboardOnScreenKeyboard::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_up->setPosition(_contentSize / 2.0f);
	_right->setPosition(_contentSize / 2.0f);
	_left->setPosition(_contentSize / 2.0f);
	_down->setPosition(_contentSize / 2.0f);
}

bool InputKeyboardOnScreenKeyboard::handleTouchBegin(const InputEvent &ev) {
	_touches.emplace(ev.data.id, convertToNodeSpace(ev.currentLocation));
	updateActiveButtons();
	return true;
}

bool InputKeyboardOnScreenKeyboard::handleTouchMove(const InputEvent &ev) {
	auto it = _touches.find(ev.data.id);
	if (it != _touches.end()) {
		it->second = convertToNodeSpace(ev.currentLocation);
		updateActiveButtons();
		return true;
	}
	return false;
}

bool InputKeyboardOnScreenKeyboard::handleTouchEnd(const InputEvent &ev) {
	auto it = _touches.find(ev.data.id);
	if (it != _touches.end()) {
		_touches.erase(it);
		updateActiveButtons();
		return true;
	}
	return false;
}

InputKeyboardOnScreenKeyboard::ActiveButtons InputKeyboardOnScreenKeyboard::computeActiveButtons() const {
	ActiveButtons ret;
	for (auto &it : _touches) {
		auto btn = getButtonForLocation(it.second);
		if (btn != ActiveButton::None) {
			ret.set(btn);
		}
	}
	return ret;
}

void InputKeyboardOnScreenKeyboard::updateActiveButtons() {
	auto active = computeActiveButtons();
	if (active != _activeButtons) {
		_up->setEnabled(active.test(ActiveButton::Up));
		_down->setEnabled(active.test(ActiveButton::Down));
		_left->setEnabled(active.test(ActiveButton::Left));
		_right->setEnabled(active.test(ActiveButton::Right));
		_activeButtons = active;
	}
}

InputKeyboardOnScreenKeyboard::ActiveButton InputKeyboardOnScreenKeyboard::getButtonForLocation(const Vec2 &loc) const {
	auto testButton = [&] (const Node *node, float distance) {
		auto bb = node->getBoundingBox();
		if (bb.containsPoint(loc)) {
			auto d = Vec2(bb.getMidX(), bb.getMidY()).distanceSquared(loc);
			if (isnan(distance) || d < distance) {
				return d;
			}
		}
		return -1.0f;
	};

	ActiveButton ret = ActiveButton::None;
	float distance = nan();
	float d = 0.0f;

	d = testButton(_up, distance);
	if (d >= 0.0f) {
		ret = ActiveButton::Up;
		distance = d;
	}
	d = testButton(_down, distance);
	if (d >= 0.0f) {
		ret = ActiveButton::Down;
		distance = d;
	}
	d = testButton(_right, distance);
	if (d >= 0.0f) {
		ret = ActiveButton::Right;
		distance = d;
	}
	d = testButton(_left, distance);
	if (d >= 0.0f) {
		ret = ActiveButton::Left;
		distance = d;
	}
	return ret;
}

void InputKeyboardOnScreenKeyboard::sendInputEvent(ActiveButton btn, InputKeyCode code, bool enabled) {
	if (!_director) {
		return;
	}

	InputEventData data({
		btn,
		enabled ? InputEventName::KeyPressed : InputEventName::KeyReleased,
		InputMouseButton::None,
		InputModifier::None,
		float(_currentLocation.x),
		float(_currentLocation.y)
	});

	data.key.keycode = code;

	_director->getApplication()->performOnMainThread([director = _director, data] {
		director->getInputDispatcher()->handleInputEvent(data);
	}, _director, true);
}

bool InputKeyboardTest::init() {
	if (!LayoutTest::init(LayoutName::InputKeyboardTest, "Use arrow buttons to control red box")) {
		return false;
	}

	_input = addInputListener(Rc<InputListener>::create());
	_key = _input->addKeyRecognizer([this] (const GestureData &input) {
		if (!_useUpdate) {
			switch (input.event) {
			case GestureEvent::Began:
				switch (input.input->data.key.keycode) {
				case InputKeyCode::LEFT: _layer->setPosition(_layer->getPosition() + Vec3(-8.0f, 0.0f, 0.0f)); break;
				case InputKeyCode::RIGHT: _layer->setPosition(_layer->getPosition() + Vec3(8.0f, 0.0f, 0.0f)); break;
				case InputKeyCode::DOWN: _layer->setPosition(_layer->getPosition() + Vec3(0.0f, -8.0f, 0.0f)); break;
				case InputKeyCode::UP: _layer->setPosition(_layer->getPosition() + Vec3(0.0f, 8.0f, 0.0f)); break;
				default: break;
				}
				break;
			case GestureEvent::Repeat:
				switch (input.input->data.key.keycode) {
				case InputKeyCode::LEFT: _layer->setPosition(_layer->getPosition() + Vec3(-8.0f, 0.0f, 0.0f)); break;
				case InputKeyCode::RIGHT: _layer->setPosition(_layer->getPosition() + Vec3(8.0f, 0.0f, 0.0f)); break;
				case InputKeyCode::DOWN: _layer->setPosition(_layer->getPosition() + Vec3(0.0f, -8.0f, 0.0f)); break;
				case InputKeyCode::UP: _layer->setPosition(_layer->getPosition() + Vec3(0.0f, 8.0f, 0.0f)); break;
				default: break;
				}
				break;
			case GestureEvent::Ended:
				break;
			case GestureEvent::Cancelled:
				break;
			}
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::LEFT, InputKeyCode::RIGHT, InputKeyCode::DOWN, InputKeyCode::UP}));

	_layer = addChild(Rc<Layer>::create(Color::Red_500));
	_layer->setAnchorPoint(Anchor::Middle);

	_handleUpdateCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Use update instead of repeat", _useUpdate, [this] (bool value) {
		_useUpdate = value;
	}), Node::ZOrderMax);
	_handleUpdateCheckbox->setAnchorPoint(Anchor::MiddleBottom);

	_onScreenKeyboard = addChild(Rc<InputKeyboardOnScreenKeyboard>::create(), ZOrder(10));
	_onScreenKeyboard->setAnchorPoint(Anchor::Middle);

	scheduleUpdate();

	return true;
}

void InputKeyboardTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	if (_layer->getContentSize() == Size2::ZERO) {
		_layer->setContentSize(Size2(50, 50));
		_layer->setPosition(_contentSize / 2.0f);
	}

	_handleUpdateCheckbox->setPosition(Vec2(_contentSize.width / 2.0f - 192.0f, 16.0f));
	_onScreenKeyboard->setPosition(Vec2(_contentSize.width - 80.0f, 120.0f));
}

void InputKeyboardTest::update(const UpdateTime &time) {
	LayoutTest::update(time);

	float speed = 128.0f;

	Vec3 offset;
	if (_useUpdate) {
		if (_key->isKeyPressed(InputKeyCode::LEFT)) {
			offset += Vec3(-speed * time.dt, 0.0f, 0.0f);
		}
		if (_key->isKeyPressed(InputKeyCode::RIGHT)) {
			offset += Vec3(speed * time.dt, 0.0f, 0.0f);
		}
		if (_key->isKeyPressed(InputKeyCode::DOWN)) {
			offset += Vec3(0.0f, -speed * time.dt, 0.0f);
		}
		if (_key->isKeyPressed(InputKeyCode::UP)) {
			offset += Vec3(0.0f, speed * time.dt, 0.0f);
		}
	}
	_layer->setPosition(_layer->getPosition() + offset);
}

}
