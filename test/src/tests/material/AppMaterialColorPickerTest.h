/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_
#define TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_

#include "MaterialSurface.h"
#include "MaterialColorScheme.h"
#include "AppMaterialTest.h"
#include "AppMaterialColorPicker.h"
#include "AppSlider.h"
#include "XLGuiLayerRounded.h"

namespace stappler::xenolith::app {

class MaterialColorSchemeNode : public Layer {
public:
	virtual ~MaterialColorSchemeNode() { }

	virtual bool init(material::ColorRole);

	virtual void onContentSizeDirty() override;

	virtual void setSchemeColor(material::ThemeType, Color4F background, Color4F label);

protected:
	void updateLabels();

	Label *_labelName = nullptr;
	Label *_labelDesc = nullptr;
	material::ThemeType _type;
	material::ColorRole _name;
};

class MaterialColorPickerTest : public MaterialTest {
public:
	virtual ~MaterialColorPickerTest() { }

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void onContentSizeDirty() override;

protected:
	using MaterialTest::init;

	void updateColor(material::ColorHCT &&);

	material::StyleContainer *_style = nullptr;
	material::Surface *_background = nullptr;
	AppCheckboxWithLabel *_lightCheckbox = nullptr;
	AppCheckboxWithLabel *_contentCheckbox = nullptr;

	MaterialColorPicker *_huePicker = nullptr;
	MaterialColorPicker *_chromaPicker = nullptr;
	MaterialColorPicker *_tonePicker = nullptr;

	LayerRounded *_spriteLayer = nullptr;
	material::ColorHCT _colorHct;
	material::ColorScheme _colorScheme;
	material::ThemeType _themeType = material::ThemeType::LightTheme;
	bool _isContentColor = false;

	std::array<MaterialColorSchemeNode *, toInt(material::ColorRole::Max)> _nodes;
};

}

#endif /* TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_ */
