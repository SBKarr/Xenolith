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

#include "AppLayoutTest.h"
#include "XLVectorSprite.h"
#include "XLIconNames.h"

namespace stappler::xenolith::app {

class LayoutTestBackButton : public VectorSprite {
public:
	virtual bool init(Function<void()> &&);

	virtual void onContentSizeDirty() override;

protected:
	void handleMouseEnter();
	void handleMouseLeave();
	bool handlePress();

	Layer *_background = nullptr;
	Function<void()> _callback;
};


bool LayoutTestBackButton::init(Function<void()> &&cb) {
	auto image = Rc<VectorImage>::create(Size2(24.0f, 24.0f));

	getIconData(IconName::Navigation_close_solid, [&] (BytesView view) {
		image->addPath("", "org.stappler.xenolith.test.LayoutTestBackButton.Close")->setPath(view).setFillColor(Color::White);
	});

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	_background = addChild(Rc<Layer>::create(Color::Grey_100), -1);
	_background->setAnchorPoint(Anchor::Middle);

	setColor(Color::Grey_600);

	auto l = addInputListener(Rc<InputListener>::create());
	l->setTouchFilter([] (const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return true;
	});
	l->addMouseOverRecognizer([this] (const GestureData &ev) {
		if (ev.event == GestureEvent::Began) {
			handleMouseEnter();
		} else {
			handleMouseLeave();
		}
		return true;
	});
	l->addPressRecognizer([this] (const GesturePress &press) {
		switch (press.event) {
		case GestureEvent::Began:
			return isTouched(press.pos);
			break;
		case GestureEvent::Activated:
			return true;
			break;
		case GestureEvent::Ended:
			return handlePress();
			break;
		case GestureEvent::Cancelled:
			return true;
			break;
		}
		return false;
	});

	_callback = move(cb);

	return true;
}

void LayoutTestBackButton::onContentSizeDirty() {
	VectorSprite::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
}

void LayoutTestBackButton::handleMouseEnter() {
	_background->stopAllActions();
	_background->runAction(Rc<TintTo>::create(0.15f, Color::Grey_400));
}

void LayoutTestBackButton::handleMouseLeave() {
	_background->stopAllActions();
	_background->runAction(Rc<TintTo>::create(0.15f, Color::Grey_100));
}

bool LayoutTestBackButton::handlePress() {
	if (_callback) {
		_callback();
	}
	return true;
}


bool LayoutTest::init(LayoutName layout, StringView text) {
	if (!Node::init()) {
		return false;
	}

	_layout = layout;
	_layoutRoot = getRootLayoutForLayout(layout);

	_backButton = addChild(Rc<LayoutTestBackButton>::create([this] {
		auto scene = dynamic_cast<AppScene *>(_scene);
		if (scene) {
			scene->runLayout(_layoutRoot, makeLayoutNode(_layoutRoot));
		}
	}), ZOrderMax);
	_backButton->setContentSize(Size2(36.0f, 36.0f));
	_backButton->setAnchorPoint(Anchor::TopRight);

	_infoLabel = addChild(Rc<Label>::create(), ZOrderMax);
	_infoLabel->setString(text);
	_infoLabel->setAnchorPoint(Anchor::MiddleTop);
	_infoLabel->setAlignment(Label::Alignment::Center);
	_infoLabel->setFontSize(24);
	_infoLabel->setAdjustValue(16);
	_infoLabel->setMaxLines(4);

	setName(getLayoutNameId(_layout));

	return true;
}

void LayoutTest::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_backButton->setPosition(_contentSize);
	_infoLabel->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 16.0f));
	_infoLabel->setWidth(_contentSize.width * 3.0f / 4.0f);
}

void LayoutTest::onEnter(Scene *scene) {
	Node::onEnter(scene);

	if (auto s = dynamic_cast<AppScene *>(scene)) {
		s->setActiveLayoutId(getName(), Value(getDataValue()));
	}
}

void LayoutTest::setDataValue(Value &&val) {
	Node::setDataValue(move(val));

	if (_running) {
		if (auto s = dynamic_cast<AppScene *>(_scene)) {
			s->setActiveLayoutId(getName(), Value(getDataValue()));
		}
	}
}

}
