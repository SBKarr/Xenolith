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

#include "AppInputTapPressTest.h"

namespace stappler::xenolith::app {

bool InputTapPressTestNode::init(StringView text) {
	auto color = Color::Red_500;
	if (!Layer::init(Color::Red_500)) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), 1);
	_label->setString(toString(text, ": ", _index));
	_label->setAnchorPoint(Anchor::Middle);
	_label->setFontSize(24);
	_label->setFontWeight(Label::FontWeight::Bold);
	_label->setColor(color.text());

	_text = text.str<Interface>();

	return true;
}

void InputTapPressTestNode::onContentSizeDirty() {
	Layer::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

void InputTapPressTestNode::handleTap() {
	++ _index;

	Color color(Color::Tone(_index % 16), Color::Level::b500);

	setColor(color);
	_label->setColor(color.text());
	_label->setString(toString(_text, ": ", _index));
}

bool InputTapPressTest::init() {
	if (!LayoutTest::init(LayoutName::InputTapPressTest, "Tap on node to change its color")) {
		return false;
	}

	InputListener *l = nullptr;

	_nodeTap = addChild(Rc<InputTapPressTestNode>::create("Tap"));
	_nodeTap->setAnchorPoint(Anchor::Middle);
	l = _nodeTap->addInputListener(Rc<InputListener>::create());
	l->addTapRecognizer([node = _nodeTap] (GestureEvent ev, const GestureTap &tap) {
		if (ev == GestureEvent::Activated && tap.count == 1) {
			node->handleTap();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	_nodeDoubleTap = addChild(Rc<InputTapPressTestNode>::create("Double tap"));
	_nodeDoubleTap->setAnchorPoint(Anchor::Middle);
	l = _nodeDoubleTap->addInputListener(Rc<InputListener>::create());
	l->addTapRecognizer([node = _nodeDoubleTap] (GestureEvent ev, const GestureTap &tap) {
		if (ev == GestureEvent::Activated && tap.count == 2) {
			node->handleTap();
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 2);

	_nodePress = addChild(Rc<InputTapPressTestNode>::create("Press"));
	_nodePress->setAnchorPoint(Anchor::Middle);
	l = _nodePress->addInputListener(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodePress] (GestureEvent ev, const GesturePress &tap) {
		if (ev == GestureEvent::Ended) {
			node->handleTap();
		}
		return true;
	});

	_nodeLongPress = addChild(Rc<InputTapPressTestNode>::create("Long press"));
	_nodeLongPress->setAnchorPoint(Anchor::Middle);
	l = _nodeLongPress->addInputListener(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodeLongPress] (GestureEvent ev, const GesturePress &tap) {
		if (ev == GestureEvent::Activated) {
			node->handleTap();
		}
		return true;
	});

	_nodeTick = addChild(Rc<InputTapPressTestNode>::create("Press tick"));
	_nodeTick->setAnchorPoint(Anchor::Middle);
	l = _nodeTick->addInputListener(Rc<InputListener>::create());
	l->addPressRecognizer([node = _nodeTick] (GestureEvent ev, const GesturePress &tap) {
		if (ev == GestureEvent::Activated) {
			node->handleTap();
		}
		return true;
	}, TapIntervalAllowed, true);

	return true;
}

void InputTapPressTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	float nodeHeight = 64.0f;

	Vec2 center(_contentSize / 2.0f);
	Size2 nodeSize(std::min(_contentSize.width / 2.0f, 256.0f), nodeHeight);

	_nodeTap->setContentSize(nodeSize);
	_nodeTap->setPosition(center + Vec2(0.0f, ((nodeHeight + 4.0f) / 2.0f) * 3.0f));

	_nodeDoubleTap->setContentSize(nodeSize);
	_nodeDoubleTap->setPosition(center + Vec2(0.0f, ((nodeHeight + 4.0f) / 2.0f) * 1.0f));

	_nodePress->setContentSize(nodeSize);
	_nodePress->setPosition(center + Vec2(0.0f, - ((nodeHeight + 4.0f) / 2.0f) * 1.0f));

	_nodeLongPress->setContentSize(nodeSize);
	_nodeLongPress->setPosition(center + Vec2(0.0f, - ((nodeHeight + 4.0f) / 2.0f) * 3.0f));

	_nodeTick->setContentSize(nodeSize);
	_nodeTick->setPosition(center + Vec2(0.0f, - ((nodeHeight + 4.0f) / 2.0f) * 5.0f));
}

}
