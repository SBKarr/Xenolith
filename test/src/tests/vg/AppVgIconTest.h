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

#ifndef TEST_SRC_TESTS_VG_APPVGICONTEST_H_
#define TEST_SRC_TESTS_VG_APPVGICONTEST_H_

#include "AppLayoutTest.h"
#include "XLIconNames.h"
#include "XLVectorSprite.h"
#include "XLGuiLayerRounded.h"
#include "AppCheckbox.h"
#include "AppSlider.h"

namespace stappler::xenolith::app {

class VgIconTest : public LayoutTest {
public:
	virtual ~VgIconTest() { }

	virtual bool init() override;

	virtual void onContentSizeDirty() override;

	virtual void update(const UpdateTime &) override;

	virtual void setDataValue(Value &&) override;

protected:
	using LayoutTest::init;

	void updateIcon(IconName);
	void updateQualityValue(float);
	void updateScaleValue(float);
	void updateAntialiasValue(bool);

	IconName _currentName = IconName::Action_text_rotate_vertical_solid;

	Label *_label = nullptr;
	Label *_info = nullptr;
	LayerRounded *_spriteLayer = nullptr;
	VectorSprite *_sprite = nullptr;
	VectorSprite *_triangles = nullptr;

	AppSliderWithLabel *_sliderQuality = nullptr;
	AppSliderWithLabel *_sliderScale = nullptr;

	AppCheckboxWithLabel *_checkboxVisible = nullptr;
	AppCheckboxWithLabel *_checkboxAntialias = nullptr;

	bool _antialias = false;
};

}

#endif /* TEST_SRC_TESTS_VG_APPVGICONTEST_H_ */
