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

namespace stappler::xenolith::app {

bool RootLayout::init() {
	if (!Node::init()) {
		return false;
	}

	_background = addChild(Rc<Layer>::create());
	_background->setColorMode(ColorMode(gl::ComponentMapping::R, gl::ComponentMapping::One));

	_logo = addChild(Rc<Sprite>::create("Xenolith.png"));
	_logo->setOpacity(0.5f);
	_logo->setContentSize(Size(308, 249));

	scheduleUpdate();

	return true;
}

void RootLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	_background->setAnchorPoint(Anchor::Middle);
	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize);

	_logo->setAnchorPoint(Anchor::Middle);
	_logo->setPosition(_contentSize / 2.0f);
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
