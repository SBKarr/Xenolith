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

#ifndef TEST_SRC_ROOT_APPTESTS_H_
#define TEST_SRC_ROOT_APPTESTS_H_

#include "XLDefine.h"

namespace stappler::xenolith::app {

enum class LayoutName {
	Root = 256 * 0,
	GeneralTests,
	InputTests,
	ActionTests,
	VgTests,
	UtilsTests,
	MaterialTests,
	Config,

	GeneralUpdateTest = 256 * 1,
	GeneralZOrderTest,
	GeneralLabelTest,
	GeneralTransparencyTest,
	GeneralAutofitTest,
	GeneralTemporaryResourceTest,
	GeneralShadowTest,
	GeneralScissorTest,

	InputTouchTest = 256 * 2,
	InputKeyboardTest,
	InputTapPressTest,
	InputSwipeTest,
	InputTextTest,

	ActionEaseTest = 256 * 3,
	ActionMaterialTest,

	VgTessTest = 256 * 4,
	VgIconTest,
	VgIconList,

	UtilsStorageTest = 256 * 5,
	UtilsNetworkTest,

	MaterialColorPickerTest = 256 * 6,
	MaterialDynamicFontTest,
	MaterialNodeTest,
	MaterialButtonTest,
	MaterialInputFieldTest,
};

struct MenuData {
	LayoutName layout;
	String title;
};

extern Vector<MenuData> s_rootMenu;
extern Vector<MenuData> s_generalTestsMenu;
extern Vector<MenuData> s_inputTestsMenu;
extern Vector<MenuData> s_actionTestsMenu;
extern Vector<MenuData> s_vgTestsMenu;
extern Vector<MenuData> s_utilsTestsMenu;
extern Vector<MenuData> s_materialTestsMenu;

LayoutName getRootLayoutForLayout(LayoutName);
StringView getLayoutNameId(LayoutName);
LayoutName getLayoutNameById(StringView);

Rc<Node> makeLayoutNode(LayoutName);

}

#endif /* TEST_SRC_ROOT_APPTESTS_H_ */
