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

#ifndef TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_
#define TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_

#include "MaterialColorScheme.h"
#include "AppLayoutTest.h"
#include "AppCheckbox.h"
#include "AppSlider.h"
#include "XLGuiLayerRounded.h"

namespace stappler::xenolith::app {

class MaterialColorSchemeNode : public Layer {
public:
	virtual ~MaterialColorSchemeNode() { }

	virtual bool init(material::ColorRole);

	virtual void onContentSizeDirty() override;

	virtual void setSchemeColor(material::ThemeType, Color4B background, Color4B label);

protected:
	void updateLabels();

	Label *_labelName = nullptr;
	Label *_labelDesc = nullptr;
	material::ThemeType _type;
	material::ColorRole _name;
};

class MaterialColorPickerSprite : public Sprite {
public:
	enum Type {
		Hue,
		Chroma,
		Tone
	};
	static constexpr uint32_t QuadsCount = 60;

	virtual ~MaterialColorPickerSprite() { }

	virtual bool init(Type, const material::ColorHCT &, Function<void(float)> &&cb);

	virtual void onContentSizeDirty() override;

	const material::ColorHCT &getTargetColor() const;
	void setTargetColor(const material::ColorHCT &color);

	void setValue(float value);
	float getValue() const;

	void setLabelColor(const Color4F &);

protected:
	virtual void updateVertexesColor() override;
	virtual void initVertexes() override;
	virtual void updateVertexes() override;

	String makeString();
	void updateValue();

	Type _type;
	float _value = 0.0f;
	material::ColorHCT _targetColor;
	Function<void(float)> _callback;
	Label *_label = nullptr;
	Layer *_indicator = nullptr;
	InputListener *_input = nullptr;
};

class MaterialColorPickerTest : public LayoutTest {
public:
	virtual ~MaterialColorPickerTest() { }

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void onContentSizeDirty() override;

protected:
	using LayoutTest::init;

	void updateColor(material::ColorHCT &&);

	Layer *_background = nullptr;
	AppCheckboxWithLabel *_lightCheckbox = nullptr;
	AppCheckboxWithLabel *_contentCheckbox = nullptr;

	MaterialColorPickerSprite *_huePicker = nullptr;
	MaterialColorPickerSprite *_chromaPicker = nullptr;
	MaterialColorPickerSprite *_tonePicker = nullptr;

	LayerRounded *_spriteLayer = nullptr;
	material::ColorHCT _colorHct;
	material::ColorScheme _colorScheme;
	material::ThemeType _themeType = material::ThemeType::LightTheme;
	bool _isContentColor = false;

	std::array<MaterialColorSchemeNode *, toInt(material::ColorRole::Max)> _nodes;
};

}

#endif /* TEST_SRC_TESTS_MATERIAL_APPMATERIALCOLORPICKERTEST_H_ */
