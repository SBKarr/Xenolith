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

#ifndef TEST_SRC_TESTS_MATERIAL_APPMATERIALBUTTONTEST_H_
#define TEST_SRC_TESTS_MATERIAL_APPMATERIALBUTTONTEST_H_

#include "MaterialButton.h"
#include "AppLayoutTest.h"

namespace stappler::xenolith::app {

class MaterialColorPickerSprite;
class AppCheckboxWithLabel;

class MaterialButtonTest : public LayoutTest {
public:
	virtual ~MaterialButtonTest() { }

	virtual bool init() override;

	virtual void onContentSizeDirty() override;
	virtual void onEnter(Scene *) override;

protected:
	material::StyleContainer *_style = nullptr;

	material::Surface *_background = nullptr;
	material::TypescaleLabel *_label = nullptr;
	Vector<material::Button *> _buttons;

	MaterialColorPickerSprite *_huePicker = nullptr;
	AppCheckboxWithLabel *_lightCheckbox = nullptr;
};

}

#endif /* TEST_SRC_TESTS_MATERIAL_APPMATERIALBUTTONTEST_H_ */
