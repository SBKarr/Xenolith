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

#include "AppInputPinchTest.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool InputPinchTest::init() {
	if (!LayoutTest::init(LayoutName::InputPinchTest, "On PC:\n"
			"Ctrl + Right click to run gesture\n"
			"Ctrl + Shift + Right click to set origin point")) {
		return false;
	}

	_node = addChild(Rc<Layer>::create(Color::Red_500));
	_node->setAnchorPoint(Anchor::Middle);
	_node->setContentSize(Size2(48.0f, 48.0f));

	auto l = addInputListener(Rc<InputListener>::create());
	l->addPinchRecognizer([this] (const GesturePinch &pinch) {
		if (pinch.event == GestureEvent::Began) {
			_node->setPosition(convertToNodeSpace(pinch.center));
			_initialScale = _node->getScale().x;
		} else if (pinch.event == GestureEvent::Activated) {
			_node->setScale(_initialScale * pinch.scale);
		}
		return true;
	});

	return true;
}

void InputPinchTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_node->setPosition(_contentSize / 2.0f);
}

}
