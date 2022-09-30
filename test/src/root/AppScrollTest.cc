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

#include "AppScrollTest.h"
#include "XLGuiScrollController.h"
#include "XLGuiScrollView.h"

namespace stappler::xenolith::app {

bool ScrollTest::init() {
	if (!Node::init()) {
		return false;
	}

	_vertical = addChild(Rc<ScrollView>::create(ScrollView::Vertical));
	_vertical->setAnchorPoint(Anchor::TopLeft);
	_vertical->setIndicatorColor(Color::Black);
	_vertical->enableScissor();
	auto controller = _vertical->setController(Rc<ScrollController>::create());

	for (uint32_t i = 0; i <= 16; ++ i) {
		auto color = Color(Color::Tone(i), Color::Level::b500);
		controller->addItem([this, color] (const ScrollController::Item &item) {
			auto l = Rc<Layer>::create(color);
			l->setVisible(false);
			return l;
		}, 128.0f);
	}

	/*_layer1 = addChild(Rc<LayerRounded>::create(Color::Red_500, 16.0f));
	_layer1->setContentSize(Size2(60, 60));
	_layer1->setAnchorPoint(Anchor::Middle);
	_layer1->setPosition(Vec2(250, 300));

	auto l1 = _layer1->addInputListener(Rc<InputListener>::create());
	l1->addSwipeRecognizer([this] (GestureEvent ev, const GestureSwipe &s) -> bool {
		switch (ev) {
		case GestureEvent::Began:
			break;
		case GestureEvent::Activated:
			_layer1->setPosition(_layer1->getPosition().xy() + s.delta);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			break;
		}
		return true;
	});
	l1->addTapRecognizer([this] (GestureEvent ev, const GestureTap &s) {
		_layer1->stopAllActions();
		if (_layer1->getOpacity() < 1.0f) {
			_layer1->runAction(Rc<FadeTo>::create(1.0f, 1.0f));
		} else {
			_layer1->runAction(Rc<FadeTo>::create(1.0f, 0.5f));
		}
		return true;
	});

	_layer2 = addChild(Rc<Layer>::create(Color::Blue_500));
	_layer2->setContentSize(Size2(60, 60));
	_layer2->setAnchorPoint(Anchor::Middle);
	_layer2->setPosition(Vec2(250, 600));

	auto l2 = _layer2->addInputListener(Rc<InputListener>::create());
	l2->addSwipeRecognizer([this] (GestureEvent ev, const GestureSwipe &s) -> bool {
		switch (ev) {
		case GestureEvent::Began:
			break;
		case GestureEvent::Activated:
			_layer1->setPosition(_layer2->getPosition().xy() + s.delta);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			break;
		}
		return true;
	});
	l2->addTapRecognizer([this] (GestureEvent ev, const GestureTap &s) {
		_layer2->stopAllActions();
		if (_layer2->getOpacity() < 1.0f) {
			_layer2->setOpacity(1.0f);
			//_layer2->runAction(Rc<FadeTo>::create(1.0f, 1.0f));
		} else {
			_layer2->setOpacity(0.5f);
			//_layer2->runAction(Rc<FadeTo>::create(1.0f, 0.5f));
		}
		return true;
	});*/

	return true;
}

void ScrollTest::onContentSizeDirty() {
	Node::onContentSizeDirty();

	if (_vertical) {
		_vertical->setPosition(Vec2(8.0f, _contentSize.height - 8.0f));
		_vertical->setContentSize(Size2(200.0f, _contentSize.height - 16.0f));
	}
}

}
