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

#ifndef TEST_SRC_TESTS_INPUT_APPINPUTKEYBOARDTEST_H_
#define TEST_SRC_TESTS_INPUT_APPINPUTKEYBOARDTEST_H_

#include "AppLayoutTest.h"

namespace stappler::xenolith::app {

class InputKeyboardTest : public LayoutTest {
public:
	virtual ~InputKeyboardTest() { }

	virtual bool init();

	virtual void onContentSizeDirty() override;

	virtual void update(const UpdateTime &) override;

protected:
	bool _useUpdate = false;
	InputListener *_input = nullptr;
	GestureKeyRecognizer *_key = nullptr;
	Layer *_layer = nullptr;
	Node *_handleUpdateCheckbox = nullptr;
	Node *_onScreenKeyboard = nullptr;

};

}

#endif /* TEST_SRC_TESTS_INPUT_APPINPUTKEYBOARDTEST_H_ */
