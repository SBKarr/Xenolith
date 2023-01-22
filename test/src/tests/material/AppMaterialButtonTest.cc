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

#include "AppMaterialButtonTest.h"
#include "AppMaterialColorPickerTest.h"

#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"

namespace stappler::xenolith::app {

bool MaterialButtonTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialButtonTest, "")) {
		return false;
	}

	_background = addChild(Rc<MaterialBackground>::create(Color::Red_500), -1);
	_background->setAnchorPoint(Anchor::Middle);

	_label = _background->addChild(Rc<material::TypescaleLabel>::create(material::TypescaleRole::HeadlineSmall), 1);
	_label->setString("None");
	_label->setAnchorPoint(Anchor::Middle);

	material::NodeStyle styles[] = {
		material::NodeStyle::Elevated,
		material::NodeStyle::Filled,
		material::NodeStyle::FilledTonal,
		material::NodeStyle::Outlined,
		material::NodeStyle::Text,
	};

	uint32_t i = 1;
	for (auto it : styles) {
		auto btn = _background->addChild(Rc<material::Button>::create(it),1);
		btn->setText(toString("Button", i));
		btn->setAnchorPoint(Anchor::Middle);
		btn->setTapCallback([this, i] { _label->setString(toString("Button", i, " Tap")); });
		btn->setLongPressCallback([this, i] { _label->setString(toString("Button", i, " Long press")); });
		btn->setDoubleTapCallback([this, i] { _label->setString(toString("Button", i, " Double tap")); });
		btn->setLeadingIconName(IconName::Navigation_check_solid);
		_buttons.emplace_back(btn);

		btn = _background->addChild(Rc<material::Button>::create(it), 1);
		btn->setText(toString("Button", i));
		btn->setAnchorPoint(Anchor::Middle);
		btn->setEnabled(false);
		btn->setTrailingIconName(IconName::Navigation_close_solid);
		_buttons.emplace_back(btn);

		++ i;
	}

	return true;
}

void MaterialButtonTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
	_label->setPosition(Vec2(_contentSize / 2.0f) + Vec2(0.0f, 180.0f));

	for (uint32_t i = 0; i < _buttons.size() / 2; ++ i) {
		_buttons[i * 2 + 0]->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-64.0f, 128.0f - 48.0f * i));
		_buttons[i * 2 + 1]->setPosition(Vec2(_contentSize / 2.0f) + Vec2(64.0f, 128.0f - 48.0f * i));
	}
}

}
