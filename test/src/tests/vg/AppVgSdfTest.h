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

#ifndef TEST_SRC_TESTS_VG_APPVGSDFTEST_H_
#define TEST_SRC_TESTS_VG_APPVGSDFTEST_H_

#include "AppLayoutTest.h"
#include "XLVectorSprite.h"

namespace stappler::xenolith::app {

class VgSdfTest : public LayoutTest {
public:
	class LightNormalSelector;

	virtual ~VgSdfTest() { }

	virtual bool init() override;
	virtual void onEnter(Scene *) override;
	virtual void onContentSizeDirty() override;

protected:
	using LayoutTest::init;

	VectorSprite *_circleSprite = nullptr;
	VectorSprite *_circleTestSprite = nullptr;
	VectorSprite *_rectSprite = nullptr;
	VectorSprite *_rectTestSprite = nullptr;
	VectorSprite *_roundedRectSprite = nullptr;
	VectorSprite *_roundedRectTestSprite = nullptr;
	VectorSprite *_triangleSprite = nullptr;
	VectorSprite *_triangleTestSprite = nullptr;
	VectorSprite *_polygonSprite = nullptr;
	VectorSprite *_polygonTestSprite = nullptr;

	AppSliderWithLabel *_sliderScaleX = nullptr;
	AppSliderWithLabel *_sliderScaleY = nullptr;
	AppSliderWithLabel *_sliderShadow = nullptr;
	AppSliderWithLabel *_sliderRotation = nullptr;
};

}

#endif /* TEST_SRC_TESTS_VG_APPVGSDFTEST_H_ */
