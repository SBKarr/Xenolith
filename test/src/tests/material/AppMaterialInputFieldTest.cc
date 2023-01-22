/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "AppMaterialInputFieldTest.h"
#include "MaterialStyleContainer.h"

namespace stappler::xenolith::app {

bool MaterialInputFieldTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialInputFieldTest, "")) {
		return false;
	}

	_background = addChild(Rc<MaterialBackground>::create(Color::Red_500), -1);
	_background->setAnchorPoint(Anchor::Middle);

	_field = _background->addChild(Rc<material::InputField>::create());
	_field->setLabelText("Label text");
	_field->setSupportingText("Supporting text");
	_field->setLeadingIconName(IconName::Action_search_solid);
	_field->setTrailingIconName(IconName::Alert_error_solid);
	_field->setContentSize(Size2(300.0f, 56.0f));
	_field->setAnchorPoint(Anchor::Middle);

	return true;
}

void MaterialInputFieldTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
	_field->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 100.0f));
}

}
