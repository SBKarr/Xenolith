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

#include "AppGeneralScissorTest.h"
#include "AppGeneralAutofitTest.h"
#include "XLLayer.h"
#include "XLInputListener.h"

namespace stappler::xenolith::app {

bool GeneralScissorTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralScissorTest, "")) {
		return false;
	}

	_node = addChild(Rc<DynamicStateNode>::create());
	_node->setAnchorPoint(Anchor::Middle);
	_node->enableScissor();
	_node->setOnContentSizeDirtyCallback([this] {
		_layer->setPosition(_node->getContentSize() / 2.0f);
	});

	_layer = _node->addChild(Rc<Layer>::create(Color::Red_500));
	_layer->setAnchorPoint(Anchor::Middle);

	_nodeResize = addChild(Rc<GeneralAutofitTestResize>::create(), ZOrder(1));
	_nodeResize->setAnchorPoint(Anchor::Middle);
	_nodeResize->setColor(Color::Grey_400);
	_nodeResize->setContentSize(Size2(48, 48));
	_nodeResize->setRotation(-45.0_to_rad);

	auto l = _nodeResize->addInputListener(Rc<InputListener>::create());
	l->addMouseOverRecognizer([this] (const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			_nodeResize->setColor(Color::Grey_600);
			break;
		default:
			_nodeResize->setColor(Color::Grey_400);
			break;
		}
		return true;
	});
	l->addSwipeRecognizer([this] (const GestureSwipe &swipe) {
		if (swipe.event == GestureEvent::Activated) {
			auto tmp = _contentSize * 0.90f * 0.5f;
			auto max = Vec2(_contentSize / 2.0f) + Vec2(tmp.width, -tmp.height);
			auto min = Vec2(_contentSize / 2.0f) + Vec2(32.0f, -32.0f);
			auto pos = _nodeResize->getPosition().xy();
			auto newPos = pos + swipe.delta / swipe.density;

			if (newPos.x < min.x) {
				newPos.x = min.x;
			}
			if (newPos.x > max.x) {
				newPos.x = max.x;
			}
			if (newPos.y > min.y) {
				newPos.y = min.y;
			}
			if (newPos.y < max.y) {
				newPos.y = max.y;
			}
			_nodeResize->setPosition(newPos);

			auto newContentSize = Size2(newPos.x - _contentSize.width / 2.0f, _contentSize.height / 2.0f - newPos.y);
			_node->setContentSize(newContentSize * 2.0f);
		}

		return true;
	});

	return true;
}

void GeneralScissorTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_layer->setContentSize(_contentSize);

	_node->setPosition(_contentSize / 2.0f);
	_node->setContentSize(_contentSize * 0.90f);

	auto tmp = _node->getContentSize() / 2.0f;

	_nodeResize->setPosition(Vec2(_contentSize / 2.0f) + Vec2(tmp.width, -tmp.height));
}

}
