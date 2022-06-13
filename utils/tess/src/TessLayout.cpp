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
#include "TessLayout.h"
#include "TessAppDelegate.h"
#include "TessCanvas.h"

namespace stappler::xenolith::tessapp {

bool TessLayout::init() {
	if (!Node::init()) {
		return false;
	}

	// auto app = (AppDelegate *)Application::getInstance();
	// auto fontController = app->getFontController();

	_background = addChild(Rc<Layer>::create(Color::White), -1);
	_background->setColorMode(ColorMode::IntensityChannel);
	_background->setAnchorPoint(Anchor::Middle);

	_canvas = addChild(Rc<TessCanvas>::create());
	_canvas->setAnchorPoint(Anchor::Middle);

	return true;
}

void TessLayout::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	if (_background) {
		_background->setPosition(center);
		_background->setContentSize(_contentSize);
	}

	if (_canvas) {
		_canvas->setPosition(center);
		_canvas->setContentSize(_contentSize);
	}
}

}
