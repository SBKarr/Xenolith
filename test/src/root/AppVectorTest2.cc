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

#include "AppVectorTest2.h"
#include "XLVectorImage.h"
#include "XLTestAppDelegate.h"

namespace stappler::xenolith::app {

bool VectorTest2::init() {
	if (!Node::init()) {
		return false;
	}

	auto app = (AppDelegate *)Application::getInstance();
	auto fontController = app->getFontController();

	do {
		auto image = Rc<VectorImage>::create(Size(100, 150));
		auto path = image->addPath();
		path->setFillColor(Color::Red_500);
		path->moveTo(100, 150).lineTo(0, 150).lineTo(100, 0).lineTo(0, 0).closePath();
		//path->addOval(Rect(0, 0, 100, 100));
		//path->addOval(Rect(0, 50, 100, 100));
		path->setWindingRule(Winding::EvenOdd);
		path->setAntialiased(false);

		_sprite = addChild(Rc<VectorSprite>::create(move(image)));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setQuality(VectorSprite::QualityLow);
		//_sprite->setLineWidth(1.0f);
	} while (0);

	/*do {
		auto image = Rc<VectorImage>::create(Size(100, 150));
		auto path = image->addPath();
		path->setFillColor(Color::Red_500);
		path->moveTo(0, 0).lineTo(100, 150).lineTo(0, 150).lineTo(100, 0).closePath();
		//path->addOval(Rect(0, 0, 100, 100));
		//path->addOval(Rect(0, 50, 100, 100));
		path->setWindingRule(Winding::NonZero);
		path->setAntialiased(false);

		_sprite2 = addChild(Rc<VectorSprite>::create(move(image)));
		_sprite2->setAnchorPoint(Anchor::Middle);
		_sprite2->setQuality(VectorSprite::QualityLow);
		_sprite2->setLineWidth(1.0f);
	} while (0);*/

	return true;
}

void VectorTest2::onContentSizeDirty() {
	Node::onContentSizeDirty();

	Size size(_contentSize * 0.3f);
	_sprite->setPosition(Vec2(_contentSize / 2.0f) - Vec2(_contentSize.width / 4.0f, 0.0f));
	//_sprite2->setPosition(Vec2(_contentSize / 2.0f) + Vec2(_contentSize.width / 4.0f, 0.0f));
}

}
