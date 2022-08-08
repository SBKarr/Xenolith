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
		//_sprite->setQuality(0.1f);
	} while (0);


	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_triangles = addChild(Rc<VectorSprite>::create(move(image)), 1);
		_triangles->setContentSize(Size2(256, 256));
		_triangles->setAnchorPoint(Anchor::Middle);
		_triangles->setColor(Color::Green_500);
		_triangles->setOpacity(0.5f);
		_triangles->setLineWidth(1.0f);
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
	_label->setString(getIconName(_currentName));

	StringView pathData(
		//"M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2z"
		//"M12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8z"
		"M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm0-6c-2.33 0-4.32 1.45-5.12 3.5h1.67c.69-1.19 1.97-2 3.45-2s2.75.81 3.45 2h1.67c-.8-2.05-2.79-3.5-5.12-3.5z"
	);

	do {
		_sprite->clear();
		auto path = _sprite->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);

	do {
		_triangles->clear();
		auto path = _triangles->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);
}

}
