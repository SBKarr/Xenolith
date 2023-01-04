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

#include "AppMaterialDynamicFontTest.h"
#include "XLLabel.h"

namespace stappler::xenolith::app {

bool MaterialDynamicFontTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialDynamicFontTest, "")) {
		return false;
	}

	float initialFontSize = 28.0f;
	float initialFontWeight = 400.0f;
	float initialFontWidth = 200.0f;
	float initialFontStyle = 0.0f;

	float minFontGrade = -200.0f;
	float maxFontGrade = 150.0f;
	float initialFontGrade = 0.0f;

	_label = addChild(Rc<Label>::create(
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
			"abcdefghijklmnopqrstuvwxyz\n"
			"1234567890!@#$%^&*(){}[],./\\"));
	_label->setFontSize(uint16_t(std::floor(initialFontSize)));
	_label->setFontFamily("sans");
	_label->setAnchorPoint(Anchor::Middle);
	_label->setAlignment(Label::Alignment::Center);
	_label->setFontStyle(Label::FontStyle::FromDegrees(initialFontStyle));

	_sliderSize = addChild(Rc<AppSliderWithLabel>::create(toString("FontSize: ", std::floor(initialFontSize)),
			(initialFontSize - 28.0f) / 100.0f, [this] (float val) {
		_sliderSize->setString(toString("FontSize: ", std::floor(val * 100.0f + 28.0f)));
		_label->setFontSize(uint16_t(std::floor(val * 100.0f + 28.0f)));
	}));
	_sliderSize->setAnchorPoint(Anchor::TopLeft);
	_sliderSize->setContentSize(Size2(128.0f, 32.0f));

	_sliderWeight = addChild(Rc<AppSliderWithLabel>::create(toString("FontWeight: ", std::floor(initialFontWeight)),
			(initialFontWeight - 100.0f) / 900.0f, [this] (float val) {
		_sliderWeight->setString(toString("FontWeight: ", std::floor(val * 900.0f + 100.0f)));
		_label->setFontWeight(Label::FontWeight(std::floor(val * 900.0f + 100.0f)));
	}));
	_sliderWeight->setAnchorPoint(Anchor::TopLeft);
	_sliderWeight->setContentSize(Size2(128.0f, 32.0f));

	_sliderWidth = addChild(Rc<AppSliderWithLabel>::create(toString("FontWidth: ", std::floor(initialFontWidth) / 2.0f),
			(initialFontWidth / 2.0f - 25.0f) / (150.0f - 25.0f), [this] (float val) {
		_sliderWidth->setString(toString("FontWidth: ", std::floor(val * (150.0f - 25.0f) + 25.0f)));
		_label->setFontStretch(Label::FontStretch(std::floor(val * (150.0f - 25.0f) + 25.0f) * 2.0f));
	}));
	_sliderWidth->setAnchorPoint(Anchor::TopLeft);
	_sliderWidth->setContentSize(Size2(128.0f, 32.0f));

	_sliderStyle = addChild(Rc<AppSliderWithLabel>::create(toString("FontStyle: ", -initialFontStyle),
			-initialFontStyle / 10.0f, [this] (float val) {
		_sliderStyle->setString(toString("FontStyle: ", val * 10.0f));
		_label->setFontStyle(Label::FontStyle::FromDegrees(-val * 10.0f));
	}));
	_sliderStyle->setAnchorPoint(Anchor::TopLeft);
	_sliderStyle->setContentSize(Size2(128.0f, 32.0f));

	_sliderGrade = addChild(Rc<AppSliderWithLabel>::create(toString("FontGrade: ", initialFontGrade),
			(initialFontGrade - minFontGrade) / (maxFontGrade - minFontGrade), [this, minFontGrade, maxFontGrade] (float val) {
		_sliderGrade->setString(toString("FontGrade: ", val * (maxFontGrade - minFontGrade) + minFontGrade));
		_label->setFontGrade(Label::FontGrade(val * (maxFontGrade - minFontGrade) + minFontGrade));
	}));
	_sliderGrade->setAnchorPoint(Anchor::TopLeft);
	_sliderGrade->setContentSize(Size2(128.0f, 32.0f));

	return true;
}

void MaterialDynamicFontTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
	_label->setWidth(_contentSize.width);

	_sliderSize->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_sliderWeight->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));
	_sliderGrade->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f * 2.0f));
	_sliderWidth->setPosition(Vec2(360.0f + 16.0f, _contentSize.height - 16.0f - 48.0f * 0.0f));
	_sliderStyle->setPosition(Vec2(360.0f + 16.0f, _contentSize.height - 16.0f - 48.0f * 1.0f));
}

}
