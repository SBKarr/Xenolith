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

#include "XLDefine.h"
#include "AppRootLayout.h"
#include "XLTestAppDelegate.h"

namespace stappler::xenolith::app {

bool RootLayout::init() {
	if (!Node::init()) {
		return false;
	}

	/*_background = addChild(Rc<Layer>::create());
	_background->setColorMode(ColorMode(gl::ComponentMapping::R, gl::ComponentMapping::One));
	_background->setAnchorPoint(Anchor::Middle);*/

	/*_logo = addChild(Rc<Sprite>::create("Xenolith.png"), 6);
	_logo->setOpacity(0.5f);
	_logo->setContentSize(Size(308, 249));
	_logo->setAnchorPoint(Anchor::Middle);*/

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
		_layers[i]->setContentSize(Size(300, 300));
		_layers[i]->setColor(colors[i]);
		_layers[i]->setAnchorPoint(Anchor::Middle);
	}

	auto app = (AppDelegate *)Application::getInstance();
	auto fontController = app->getFontController();

	_label = addChild(Rc<Label>::create(fontController), 5);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setColor(Color::Green_500, true);
	_label->setOpacity(0.75f);

	_label->setFontFamily("monospace");
	_label->setFontSize(48);
	_label->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic}));
	_label->appendTextWithStyle("World", Label::Style({font::FontWeight::Bold}));

	_label2 = addChild(Rc<Label>::create(fontController), 5);
	_label2->setAnchorPoint(Anchor::Middle);
	_label2->setColor(Color::BlueGrey_500, true);
	_label2->setOpacity(0.75f);

	_label2->setFontFamily("Roboto");
	_label2->setFontSize(48);
	_label2->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic}));
	_label2->appendTextWithStyle("World", Label::Style({font::FontWeight::Bold, Color::Red_500}));

	scheduleUpdate();

	return true;
}

void RootLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	if (_background) {
		_background->setPosition(_contentSize / 2.0f);
		_background->setContentSize(_contentSize);
	}

	//_logo->setPosition(_contentSize / 2.0f);

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

void RootLayout::update(const UpdateTime &time) {
	Node::update(time);

	auto t = time.app % 5_usec;

	if (_background) {
		_background->setGradient(SimpleGradient(Color::Red_500, Color::Green_500,
				Vec2::forAngle(M_PI * 2.0 * (float(t) / 5_usec))));
	}
}

}
