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

#include "AppInputSwipeTest.h"
#include "XLGuiActionAcceleratedMove.h"

namespace stappler::xenolith::app {

bool InputSwipeTest::init() {
	if (!LayoutTest::init(LayoutName::InputSwipeTest, "Use swipe to move node within rectangle")) {
		return false;
	}

	_boundsLayer = addChild(Rc<Layer>::create(Color::Grey_100));
	_boundsLayer->setAnchorPoint(Anchor::MiddleTop);

	_node = addChild(Rc<Layer>::create(Color::Red_500));
	_node->setAnchorPoint(Anchor::Middle);
	_node->setContentSize(Size2(48.0f, 48.0f));

	auto l = addInputListener(Rc<InputListener>::create());
	l->addSwipeRecognizer([this] (const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began:
			if (_node->isTouched(swipe.input->originalLocation)) {
				_node->setColor(Color::Blue_500);
				_node->stopActionByTag(1);
				return true;
			}
			return false;
			break;
		case GestureEvent::Activated:
			_node->setPosition(getBoundedPosition(_node->getPosition().xy() + swipe.delta / swipe.density));
			return true;
			break;
		case GestureEvent::Ended:
			_node->setColor(Color::Red_500);
			_node->runAction(ActionAcceleratedMove::createWithBounds(
					5000, _node->getPosition().xy(), swipe.velocity / swipe.density, getBoundsRect()), 1);
			break;
		case GestureEvent::Cancelled:
			_node->setColor(Color::Red_500);
			break;
		default:
			break;
		}
		return false;
	});

	return true;
}

void InputSwipeTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_boundsLayer->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 64.0f));
	_boundsLayer->setContentSize(_contentSize - Size2(64.0f, 96.0f));

	if (_node->getPosition() == Vec3::ZERO) {
		_node->setPosition(_contentSize / 2.0f);
	}
}

Vec2 InputSwipeTest::getBoundedPosition(Vec2 pt) const {
	auto bbox = getBoundsRect();

	if (pt.x < bbox.origin.x) {
		pt.x = bbox.origin.x;
	}
	if (pt.y < bbox.origin.y) {
		pt.y = bbox.origin.y;
	}
	if (pt.x > bbox.origin.x + bbox.size.width) {
		pt.x = bbox.origin.x + bbox.size.width;
	}
	if (pt.y > bbox.origin.y + bbox.size.height) {
		pt.y = bbox.origin.y + bbox.size.height;
	}
	return pt;
}

Rect InputSwipeTest::getBoundsRect() const {
	auto bbox = _boundsLayer->getBoundingBox();

	bbox.origin += _node->getContentSize() / 2.0f;
	bbox.size = bbox.size - _node->getContentSize();
	return bbox;
}

}
