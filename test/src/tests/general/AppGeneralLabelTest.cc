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

#include "AppGeneralLabelTest.h"
#include "XLLabel.h"

namespace stappler::xenolith::app {

bool GeneralLabelTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralLabelTest, "Test for rich label functions")) {
		return false;
	}
	_label = addChild(Rc<Label>::create(), ZOrder(5));
	_label->setScale(0.5f);
	_label->setAnchorPoint(Anchor::Middle);
	_label->setColor(Color::Green_500, true);

	_label->setFontSize(48);
	_label->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic}));
	_label->appendTextWithStyle("World", Label::Style({font::FontWeight::Bold}));

	_label2 = addChild(Rc<Label>::create(), ZOrder(5));
	_label2->setAnchorPoint(Anchor::Middle);
	_label2->setColor(Color::BlueGrey_500, true);
	_label2->setOpacity(0.75f);

	_label2->setFontSize(48);
	_label2->appendTextWithStyle("Hello", Label::Style({font::FontStyle::Italic, Label::TextDecoration::LineThrough}));
	_label2->appendTextWithStyle("\nWorld", Label::Style({font::FontWeight::Bold, Color::Red_500, Label::TextDecoration::Underline}));

	return true;
}

void GeneralLabelTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	Vec2 center = _contentSize / 2.0f;

	_label->setPosition(center - Vec2(0.0f, 50.0f));
	_label2->setPosition(center + Vec2(0.0f, 50.0f));
}

}
