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

#include "AppInputTouchTest.h"
#include "XLInputListener.h"
#include "XLLayer.h"

namespace stappler::xenolith::app {

bool InputTouchTest::init() {
	if (!LayoutTest::init(LayoutName::InputTouchTest, "Click to add node, click on node to remove it\nClick on node and drag to move node")) {
		return false;
	}

	_input = addInputListener(Rc<InputListener>::create());
	_input->addScrollRecognizer([] (const GestureScroll &scroll) {
		std::cout << "Scroll: " << scroll.event << ": " << scroll.pos << " - " << scroll.amount << "\n";
		return true;
	});
	_input->addTouchRecognizer([this] (const GestureData &ev) {
		std::cout << "Touch (left): " << ev.event << ": " << ev.input->currentLocation << "\n";
		if (ev.event == GestureEvent::Ended) {
			handleClick(convertToNodeSpace(ev.input->currentLocation));
		}
		return true;
	});
	_input->addTouchRecognizer([] (const GestureData &ev) {
		std::cout << "Touch (right): " << ev.event << ": " << ev.input->currentLocation << "\n";
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseRight}));
	_input->addMoveRecognizer([this] (const GestureData &ev) {
		auto pos = convertToNodeSpace(ev.input->currentLocation);
		_cursor->setPosition(pos);
		return true;
	});

	_cursor = addChild(Rc<Layer>::create(Color::Blue_500), ZOrder(10));
	_cursor->setContentSize(Size2(10, 10));
	_cursor->setAnchorPoint(Anchor::Middle);

	return true;
}

void InputTouchTest::handleClick(const Vec2 &loc) {
	Color color(Color::Tone(_accum ++ % 16), Color::Level::b500);

	auto node = addChild(Rc<Layer>::create(color), ZOrder(9));

	node->setContentSize(Size2(50, 50));
	node->setAnchorPoint(Anchor::Middle);
	node->setPosition(loc);

	auto l = node->addInputListener(Rc<InputListener>::create());
	l->setSwallowAllEvents();
	l->addTouchRecognizer([node] (const GestureData &ev) {
		std::cout << "Touch (node): " << ev.event << ": " << ev.input->currentLocation << "\n";
		switch (ev.event) {
		case GestureEvent::Began:
			break;
		case GestureEvent::Moved:
			node->setPosition(node->getParent()->convertToNodeSpace(ev.input->currentLocation));
			break;
		case GestureEvent::Ended:
			if (node->isTouched(ev.input->currentLocation) && ev.input->currentLocation.isWithinDistance(ev.input->originalLocation, 8.0f)) {
				node->removeFromParent();
			}
			break;
		case GestureEvent::Cancelled:
			break;
		}
		return true;
	});
	l->addScrollRecognizer([node] (const GestureScroll &scroll) {
		if (scroll.amount.y != 0.0f) {
			auto zRot = node->getRotation();
			node->setRotation(zRot + scroll.amount.y / 40.0f);
		}
		std::cout << "Scroll: " << scroll.event << ": " << scroll.pos << " - " << scroll.amount << "\n";
		return true;
	});
}

}
