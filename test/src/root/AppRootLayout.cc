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

#include "AppRootLayout.h"
#include "XLInputListener.h"
#include "XLAction.h"

namespace stappler::xenolith::app {

class RootLayoutButton : public Node {
public:
	virtual ~RootLayoutButton() { }

	virtual bool init(StringView);

	virtual void onContentSizeDirty() override;

protected:
	void handleFocusEnter();
	void handleFocusLeave();

	Layer *_layer = nullptr;
	Label *_label = nullptr;
	bool _focus = false;
};

bool RootLayoutButton::init(StringView str) {
	if (!Node::init()) {
		return false;
	}

	_label = addChild(Rc<Label>::create(str), 2);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(48);
	_label->setOnContentSizeDirtyCallback([this] {
		setContentSize(_label->getContentSize() + Size2(20.0f, 20.0f));
	});

	_layer = addChild(Rc<Layer>::create(Color::Grey_200));
	_layer->setAnchorPoint(Anchor::BottomLeft);

	auto l = addInputListener(Rc<InputListener>::create());
	l->setTouchFilter([] (const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return true;
	});
	l->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		bool touched = isTouched(ev.currentLocation);
		if (touched != _focus) {
			_focus = touched;
			if (_focus) {
				std::cout << "handleFocusEnter\n";
				handleFocusEnter();
			} else {
				std::cout << "handleFocusLeave\n";
				handleFocusLeave();
			}
		}
		return true;
	});

	return true;
}

void RootLayoutButton::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_layer->setContentSize(_contentSize);
	_label->setPosition(_contentSize / 2.0f);
}

void RootLayoutButton::handleFocusEnter() {
	_layer->stopAllActions();
	_layer->runAction(Rc<Sequence>::create(
			Rc<Spawn>::create([] {
				log::text("Test", "test0.1");
			},[] {
				log::text("Test", "test0.2");
			},
			Rc<TintTo>::create(0.5f, Color::Red_200)),
			[] {
				log::text("Test", "test1");
			},
			1.5f,
			Rc<Spawn>::create([] {
				log::text("Test", "test2.1");
			},[] {
				log::text("Test", "test2.2");
			}),
			Rc<TintTo>::create(0.5f, Color::Blue_200),
			[] {
				log::text("Test", "test3");
			}
	));
}

void RootLayoutButton::handleFocusLeave() {
	_layer->stopAllActions();
	_layer->runAction(Rc<Sequence>::create(Rc<TintTo>::create(0.5f, Color::Grey_200)));
}

bool RootLayout::init() {
	if (!Node::init()) {
		return false;
	}

	auto btn = addChild(Rc<RootLayoutButton>::create("Test button"));
	btn->setAnchorPoint(Anchor::BottomLeft);
	btn->setPosition(Vec2(20.0f, 20.0f));

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_sprite = addChild(Rc<VectorSprite>::create(move(image)));
		_sprite->setContentSize(Size2(256, 256));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setColor(Color::Black);
		_sprite->setOpacity(0.5f);
		//_sprite->setQuality(2.75f);
	} while (0);


	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_triangles = addChild(Rc<VectorSprite>::create(move(image)), 1);
		_triangles->setContentSize(Size2(256, 256));
		_triangles->setAnchorPoint(Anchor::Middle);
		_triangles->setColor(Color::Green_500);
		_triangles->setOpacity(0.5f);
		_triangles->setLineWidth(1.0f);
		_triangles->setVisible(false);
		_triangles->setQuality(1.25f);
		//_triangles->setQuality(0.1f);
	} while (0);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(32);
	_label->setString(getIconName(_currentName));
	_label->setAnchorPoint(Anchor::MiddleTop);

	auto l = _sprite->addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (GestureEvent ev, const InputEvent &data) -> bool {
		if (ev == GestureEvent::Ended) {
			if (data.data.button == InputMouseButton::Mouse8) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (data.data.button == InputMouseButton::Mouse9) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseScrollLeft, InputMouseButton::MouseScrollRight, InputMouseButton::Mouse8, InputMouseButton::Mouse9}));

	l->addKeyRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		if (event == GestureEvent::Ended) {
			if (ev.data.key.keycode == InputKeyCode::LEFT) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (ev.data.key.keycode == InputKeyCode::RIGHT) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::LEFT, InputKeyCode::RIGHT}));


	scheduleUpdate();
	updateIcon(_currentName);

	return true;
}

void RootLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_sprite->setPosition(_contentSize / 2.0f);
	_triangles->setPosition(_contentSize / 2.0f);
	_label->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 128.0f));
}

void RootLayout::update(const UpdateTime &t) {
	Node::update(t);
}

void RootLayout::updateIcon(IconName name) {
	_currentName = name;
	_label->setString(toString(getIconName(_currentName), " ", toInt(_currentName), "/", toInt(IconName::Toggle_toggle_on_solid)));

	StringView pathData(
		//"M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2z"
		//"M12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8z"
		//"M9 13c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4z"
		//"m0-6c1.1 0 2 .9 2 2s-.9 2-2 2-2-.9-2-2 .9-2 2-2z"
		//"m0 8c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"
		//"m-6 4c.22-.72 3.31-2 6-2 2.7 0 5.8 1.29 6 2H3z"
		//"M15.08 7.05c.84 1.18.84 2.71 0 3.89l1.68 1.69 0-7.27l-1.68 1.69z"
		//"M9 15c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"
		//"M16.7600002 5.35999966l-1.68 1.69c.84 1.18.84 2.71 0 3.89l1.68 1.69c2.02-2.02 2.02-5.07 0-7.27z"
		//"M20.07 2l-1.63 1.63c2.77 3.02 2.77 7.56 0 10.74L20.07 16c3.9-3.89 3.91-9.95 0-14z"
		//"M20.07 2l-1.63 1.63c2.77 3.02 2.77 7.56 0 10.74L20.07 16c3.9-3.89 3.91-9.95 0-14z"

		//"M11 24h2v-2h-2v2z"
		//"m-4 0h2v-2H7v2z"
		//"m8 0h2v-2h-2v2z"
		//"m2.71-18.29L12 0h-1v7.59L6.41 3 5 4.41 10.59 10 5 15.59 6.41 17 11 12.41V20h1l5.71-5.71-4.3-4.29 4.3-4.29z"
		//"M13 3.83l1.88 1.88L13 7.59V3.83zm1.88 10.46L13 16.17v-3.76l1.88 1.88z"

		//"M18,2L6,2L6,14L18,14z"
		//"M10,10l-1,1l-1-1z"
		//"M12,4l2,2h-4z"
		//"M16,10l-1,1 l-1-1z"

		//"M1.41 1.69L0 3.1l1 .99V16L14 18h.9l6 6 1.41-1.41-20.9-20.9z"
		//"M2.99 16V6.09L12.9 16H2.99z"
		//"M4.55 2l2 2H21v12h-2.45l2 2h.44c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2H4.55z"

		//"M17.27 6.73l-4.24 10.13-1.32-3.42-.32-.83-.82-.32-3.43-1.33 10.13-4.23"
		"M21 3L3 10.53v.98l6.84 2.65L12.48 21h.98L21 3z"
	);

	do {
		_sprite->clear();
		auto path = _sprite->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		path->setAntialiased(false);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);

	do {
		_triangles->clear();
		auto path = _triangles->addPath();
		path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		//path->setAntialiased(false);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);
}

}
