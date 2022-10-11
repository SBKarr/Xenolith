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

#include "AppZOrderTest2.h"
#include "XLDefine.h"
#include "XLTestAppDelegate.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool ZOrderTest::init() {
	if (!Node::init()) {
		return false;
	}

	_background = addChild(Rc<Layer>::create());
	_background->setColorMode(ColorMode(gl::ComponentMapping::R, gl::ComponentMapping::One));
	_background->setAnchorPoint(Anchor::Middle);

	_logo = addChild(Rc<Sprite>::create("Xenolith.png"), 6);
	_logo->setOpacity(0.5f);
	_logo->setContentSize(Size2(308, 249));
	_logo->setAnchorPoint(Anchor::Middle);

	Color colors[5] = {
		Color::Red_500,
		Color::Green_500,
		Color::White,
		Color::Blue_500,
		Color::Teal_500
	};

	int16_t indexes[5] = {
		4, 3, 5, 2, 1
	};

	for (size_t i = 0; i < 5; ++ i) {
		_layers[i] = addChild(Rc<Layer>::create(), indexes[i]);
		_layers[i]->setContentSize(Size2(300, 300));
		_layers[i]->setColor(colors[i]);
		_layers[i]->setAnchorPoint(Anchor::Middle);
	}

	/*_label = addChild(Rc<Label>::create(fontController), 5);
	_label->setScale(0.5f);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setColor(Color::Green_500, true);
	_label->setOpacity(0.75f);

	_label->setFontFamily("monospace");
	_label->setFontSize(48);
	_label->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic}));
	_label->appendTextWithStyle("World", Label::Style({font::FontWeight::Bold}));

	auto label1Listener = _label->addInputListener(Rc<InputListener>::create());
	label1Listener->addTouchRecognizer([] (GestureEvent event, const InputEvent &ev) {
		std::cout << "Touch (Label1): " << event << ": " << ev.currentLocation << "\n";
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseRight}));*/

	_label2 = addChild(Rc<Label>::create(), 5);
	_label2->setAnchorPoint(Anchor::Middle);
	_label2->setColor(Color::BlueGrey_500, true);
	_label2->setOpacity(0.75f);

	_label2->setFontSize(48);
	_label2->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic, Label::TextDecoration::LineThrough}));
	_label2->appendTextWithStyle("\nWorld", Label::Style({font::FontWeight::Bold, Color::Red_500, Label::TextDecoration::Underline}));

	auto label2Listener = _label2->addInputListener(Rc<InputListener>::create());
	label2Listener->addTouchRecognizer([] (GestureEvent event, const InputEvent &ev) {
		std::cout << "Touch (Label2): " << event << ": " << ev.currentLocation << "\n";
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseRight}));

	_cursor = addChild(Rc<Layer>::create(Color::Blue_500), 10);
	_cursor->setContentSize(Size2(10, 10));
	_cursor->setAnchorPoint(Anchor::Middle);

	scheduleUpdate();

	auto l = addInputListener(Rc<InputListener>::create());
	l->addScrollRecognizer([] (GestureEvent event, const GestureScroll &scroll) {
		std::cout << "Scroll: " << event << ": " << scroll.pos << " - " << scroll.amount << "\n";
		return true;
	});
	l->addTouchRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		std::cout << "Touch (left): " << event << ": " << ev.currentLocation << "\n";
		if (event == GestureEvent::Ended) {
			handleClick(ev.currentLocation);
		}
		return true;
	});
	l->addTouchRecognizer([] (GestureEvent event, const InputEvent &ev) {
		std::cout << "Touch (right): " << event << ": " << ev.currentLocation << "\n";
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseRight}));
	l->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		auto pos = convertToNodeSpace(ev.currentLocation);
		_cursor->setPosition(pos);
		return true;
	});

	return true;
}

void ZOrderTest::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	if (_background) {
		_background->setPosition(_contentSize / 2.0f);
		_background->setContentSize(_contentSize);
	}

	Vec2 positions[5] = {
		center + Vec2(-100, -100),
		center + Vec2(100, -100),
		center,
		center + Vec2(-100, 100),
		center + Vec2(100, 100),
	};

	for (size_t i = 0; i < 5; ++ i) {
		if (_layers[i]) {
			_layers[i]->setPosition(positions[i]);
		}
	}

	if (_label) {
		_label->setPosition(center - Vec2(0.0f, 50.0f));
	}

	if (_label2) {
		_label2->setPosition(center + Vec2(0.0f, 50.0f));
	}
}

void ZOrderTest::update(const UpdateTime &time) {
	Node::update(time);

	auto t = time.app % 5_usec;

	if (_background) {
		_background->setGradient(SimpleGradient(Color::Red_500, Color::Green_500,
				Vec2::forAngle(M_PI * 2.0 * (float(t) / 5_usec))));
	}
}

void ZOrderTest::handleClick(const Vec2 &loc) {
	auto node = addChild(Rc<Layer>::create(Color::Grey_500), 9);

	node->setContentSize(Size2(50, 50));
	node->setAnchorPoint(Anchor::Middle);
	node->setPosition(loc);

	auto l = node->addInputListener(Rc<InputListener>::create());
	l->setSwallowAllEvents();
	l->addTouchRecognizer([node] (GestureEvent event, const InputEvent &ev) {
		std::cout << "Touch (node): " << event << ": " << ev.currentLocation << "\n";
		if (event == GestureEvent::Ended && node->isTouched(ev.currentLocation)) {
			node->removeFromParent();
		}
		return true;
	});
	l->addScrollRecognizer([node] (GestureEvent event, const GestureScroll &scroll) {
		if (scroll.amount.y != 0.0f) {
			auto zRot = node->getRotation();
			node->setRotation(zRot + scroll.amount.y / 40.0f);
		}
		std::cout << "Scroll: " << event << ": " << scroll.pos << " - " << scroll.amount << "\n";
		return true;
	});
}

}
