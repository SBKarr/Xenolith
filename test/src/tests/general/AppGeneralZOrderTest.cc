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

#include "AppGeneralZOrderTest.h"

namespace stappler::xenolith::app {

bool GeneralZOrderTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralZOrderTest, "Correct Z ordering: white, red, green, blue, teal")) {
		return false;
	}

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
		_layers[i]->setContentSize(Size2(300, 300));
		_layers[i]->setColor(colors[i]);
		_layers[i]->setAnchorPoint(Anchor::Middle);
	}

	return true;
}

void GeneralZOrderTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

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
}

}
