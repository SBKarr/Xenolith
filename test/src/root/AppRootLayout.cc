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
	_layer->runAction(Rc<TintTo>::create(0.5f, Color::Red_200));
}

void RootLayoutButton::handleFocusLeave() {
	_layer->stopAllActions();
	_layer->runAction(Rc<TintTo>::create(0.5f, Color::Grey_200));
}

bool RootLayout::init() {
	if (!Node::init()) {
		return false;
	}

	auto btn = addChild(Rc<RootLayoutButton>::create("Test button"));
	btn->setAnchorPoint(Anchor::BottomLeft);
	btn->setPosition(Vec2(20.0f, 20.0f));

	scheduleUpdate();

	return true;
}

void RootLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();
}

void RootLayout::update(const UpdateTime &t) {
	Node::update(t);
}

}
