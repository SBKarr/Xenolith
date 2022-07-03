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

#include "TessPoint.h"
#include "TessAppDelegate.h"

#include "XLLabel.h"

namespace stappler::xenolith::tessapp {

bool TessPoint::init(const Vec2 &p, uint32_t index) {
	auto image = Rc<VectorImage>::create(Size2(10, 10));
	image->addPath("", "org.stappler.xenolith.tess.TessPoint")
		->setFillColor(Color::White)
		.addOval(Rect(0, 0, 10, 10))
		.setAntialiased(false);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	auto app = (AppDelegate *)Application::getInstance();
	auto fontController = app->getFontController();

	_label = addChild(Rc<Label>::create(fontController));
	_label->setFontSize(12);
	_label->setFontFamily("monospace");
	_label->setFontWeight(Label::FontWeight::Bold);
	_label->setColor(Color::Black, true);
	_label->setString(toString(index, "; ", p.x, " ", p.y));
	_label->setPosition(Vec2(12, 12));

	setAnchorPoint(Anchor::Middle);
	setPosition(p);
	setColor(Color::Red_500);
	_point = p;
	_index = index;
	return true;
}

void TessPoint::setPoint(const Vec2 &pt) {
	_point = pt;
	setPosition(pt);
	_label->setString(toString(_index, "; ", _point.x, " ", _point.y));
}

void TessPoint::setIndex(uint32_t index) {
	if (_index != index) {
		_label->setString(toString(index, "; ", _point.x, " ", _point.y));
		_index = index;
	}
}

}
