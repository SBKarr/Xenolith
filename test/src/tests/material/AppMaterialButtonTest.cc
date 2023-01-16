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

	auto color = material::ColorHCT(Color::Red_500.asColor4F());

	_style = addComponent(Rc<material::StyleContainer>::create());
	_style->setPrimaryScheme(material::ThemeType::LightTheme, color.asColor4F(), false);

	addComponent(Rc<material::SurfaceInterior>::create(material::SurfaceStyle{material::ColorRole::Primary,
		material::Elevation::Level1, material::NodeStyle::Text
	}));

	_background = addChild(Rc<material::Surface>::create(material::SurfaceStyle::Background), -1);
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
		auto btn = addChild(Rc<material::Button>::create(material::ButtonData{
			toString("Button", i), IconName::None, IconName::None,
			[this, i] { _label->setString(toString("Button", i, " Tap")); },
			[this, i] { _label->setString(toString("Button", i, " Long press")); },
			[this, i] { _label->setString(toString("Button", i, " Double tap")); }
		}, it), 1);
		btn->setAnchorPoint(Anchor::Middle);
		_buttons.emplace_back(btn);

		btn = addChild(Rc<material::Button>::create(material::ButtonData{toString("Button", i)}, it), 1);
		btn->setAnchorPoint(Anchor::Middle);
		btn->setEnabled(false);
		_buttons.emplace_back(btn);

		++ i;
	}

	_huePicker = addChild(Rc<MaterialColorPickerSprite>::create(MaterialColorPickerSprite::Hue, color, [this] (float val) {
		auto color = material::ColorHCT(val, 100.0f, 50.0f, 1.0f);
		_style->setPrimaryScheme(_lightCheckbox->getValue() ? material::ThemeType::DarkTheme : material::ThemeType::LightTheme, color, false);
		_huePicker->setTargetColor(color);
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_lightCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Dark theme", false, [this] (bool value) {
		if (value) {
			_style->setPrimaryScheme(material::ThemeType::DarkTheme, _huePicker->getTargetColor(), false);
		} else {
			_style->setPrimaryScheme(material::ThemeType::LightTheme, _huePicker->getTargetColor(), false);
		}
	}));
	_lightCheckbox->setAnchorPoint(Anchor::TopLeft);
	_lightCheckbox->setContentSize(Size2(24.0f, 24.0f));

	return true;
}

void MaterialButtonTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
	_label->setPosition(Vec2(_contentSize / 2.0f) + Vec2(0.0f, 180.0f));

	_huePicker->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_lightCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 48.0f));

	for (uint32_t i = 0; i < _buttons.size() / 2; ++ i) {
		_buttons[i * 2 + 0]->setPosition(Vec2(_contentSize / 2.0f) + Vec2(-64.0f, 128.0f - 48.0f * i));
		_buttons[i * 2 + 1]->setPosition(Vec2(_contentSize / 2.0f) + Vec2(64.0f, 128.0f - 48.0f * i));
	}
}

void MaterialButtonTest::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.3f), 1.5f, Color::White);
	auto ambient = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, Color::White);

	_scene->setGlobalColor(Color4F::WHITE);
	_scene->removeAllLights();
	_scene->addLight(move(light));
	_scene->addLight(move(ambient));
}

}
