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

#include "AppMaterialBackground.h"
#include "MaterialStyleContainer.h"

namespace stappler::xenolith::app {

bool MaterialBackground::init(const Color4F &c) {
	if (!BackgroundSurface::init()) {
		return false;
	}

	auto color = material::ColorHCT(c);

	_styleContainer->setPrimaryScheme(material::ThemeType::LightTheme, color, false);

	_huePicker = addChild(Rc<MaterialColorPicker>::create(MaterialColorPicker::Hue, color, [this] (float val) {
		auto color = material::ColorHCT(val, 100.0f, 50.0f, 1.0f);
		_styleContainer->setPrimaryScheme(_lightCheckbox->getValue() ? material::ThemeType::DarkTheme : material::ThemeType::LightTheme, color, false);
		_huePicker->setTargetColor(color);
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_lightCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Dark theme", false, [this] (bool value) {
		if (value) {
			_styleContainer->setPrimaryScheme(material::ThemeType::DarkTheme, _huePicker->getTargetColor(), false);
		} else {
			_styleContainer->setPrimaryScheme(material::ThemeType::LightTheme, _huePicker->getTargetColor(), false);
		}
	}));
	_lightCheckbox->setAnchorPoint(Anchor::TopLeft);
	_lightCheckbox->setContentSize(Size2(24.0f, 24.0f));

	return true;
}

void MaterialBackground::onContentSizeDirty() {
	BackgroundSurface::onContentSizeDirty();

	_huePicker->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_lightCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 48.0f));
}

void MaterialBackground::onEnter(Scene *scene) {
	BackgroundSurface::onEnter(scene);

	auto color = Color4F::WHITE;
	color.a = 0.5f;

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.3f), 1.5f, color);
	auto ambient = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, color);

	_scene->setGlobalLight(Color4F::WHITE);
	_scene->removeAllLights();
	_scene->addLight(move(light));
	_scene->addLight(move(ambient));
}

}
