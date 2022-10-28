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

#include "XLDefine.h"
#include "AppTests.h"

#include "AppRootLayout.cc"

#include "general/AppGeneralLabelTest.cc"
#include "general/AppGeneralUpdateTest.cc"
#include "general/AppGeneralZOrderTest.cc"
#include "general/AppGeneralTransparencyTest.cc"
#include "general/AppGeneralAutofitTest.cc"
#include "general/AppGeneralTemporaryResourceTest.cc"
#include "input/AppInputTouchTest.cc"
#include "input/AppInputKeyboardTest.cc"
#include "input/AppInputTapPressTest.cc"
#include "input/AppInputSwipeTest.cc"
#include "input/AppInputTextTest.cc"
#include "action/AppActionEaseTest.cc"
#include "vg/AppVgTessCanvas.cc"
#include "vg/AppVgTessTest.cc"
#include "vg/AppVgIconTest.cc"
#include "vg/AppVgIconList.cc"
#include "config/AppConfigMenu.cc"
#include "config/AppConfigPresentModeSwitcher.cc"

namespace stappler::xenolith::app {

Vector<MenuData> s_rootMenu({
	MenuData{LayoutName::GeneralTests, "General tests"},
	MenuData{LayoutName::InputTests, "Input tests"},
	MenuData{LayoutName::ActionTests, "Action tests"},
	MenuData{LayoutName::VgTests, "VG tests"},
	MenuData{LayoutName::GuiTests, "Gui tests"},
	MenuData{LayoutName::Config, "Config"},
});

Vector<MenuData> s_generalTestsMenu({
	MenuData{LayoutName::GeneralUpdateTest, "Update test"},
	MenuData{LayoutName::GeneralZOrderTest, "Z order test"},
	MenuData{LayoutName::GeneralLabelTest, "Label test"},
	MenuData{LayoutName::GeneralTransparencyTest, "Transparency test"},
	MenuData{LayoutName::GeneralAutofitTest, "Autofit test"},
	MenuData{LayoutName::GeneralTemporaryResourceTest, "Temporary resource test"},
});

Vector<MenuData> s_inputTestsMenu({
	MenuData{LayoutName::InputTouchTest, "Touch test"},
	MenuData{LayoutName::InputKeyboardTest, "Keyboard test"},
	MenuData{LayoutName::InputTapPressTest, "Tap/Press test"},
	MenuData{LayoutName::InputSwipeTest, "Swipe test"},
	MenuData{LayoutName::InputTextTest, "Text test"},
});

Vector<MenuData> s_actionTestsMenu({
	MenuData{LayoutName::ActionProgressTest, "Progress test"},
	MenuData{LayoutName::ActionEaseTest, "Ease test"},
});

Vector<MenuData> s_vgTestsMenu({
	MenuData{LayoutName::VgTessTest, "Tess test"},
	MenuData{LayoutName::VgIconTest, "Icon test"},
	MenuData{LayoutName::VgIconList, "Icon list"},

});

Vector<MenuData> s_guiTestsMenu({
	MenuData{LayoutName::GuiScrollTest, "Scroll test"},
});

LayoutName getRootLayoutForLayout(LayoutName name) {
	switch (name) {
	case LayoutName::Root:
	case LayoutName::GeneralTests:
	case LayoutName::InputTests:
	case LayoutName::ActionTests:
	case LayoutName::VgTests:
	case LayoutName::GuiTests:
	case LayoutName::Config:
		return LayoutName::Root;
		break;

	case LayoutName::GeneralUpdateTest:
	case LayoutName::GeneralZOrderTest:
	case LayoutName::GeneralLabelTest:
	case LayoutName::GeneralTransparencyTest:
	case LayoutName::GeneralAutofitTest:
	case LayoutName::GeneralTemporaryResourceTest:
		return LayoutName::GeneralTests;
		break;

	case LayoutName::InputTouchTest:
	case LayoutName::InputKeyboardTest:
	case LayoutName::InputTapPressTest:
	case LayoutName::InputSwipeTest:
	case LayoutName::InputTextTest:
		return LayoutName::InputTests;
		break;

	case LayoutName::ActionProgressTest:
	case LayoutName::ActionEaseTest:
		return LayoutName::ActionTests;
		break;

	case LayoutName::VgTessTest:
	case LayoutName::VgIconTest:
	case LayoutName::VgIconList:
		return LayoutName::VgTests;
		break;

	case LayoutName::GuiScrollTest:
		return LayoutName::GuiTests;
		break;
	}
	return LayoutName::Root;
}

StringView getLayoutNameId(LayoutName name) {
	switch (name) {
	case LayoutName::Root: return "org.stappler.xenolith.test.Root"; break;
	case LayoutName::GeneralTests: return "org.stappler.xenolith.test.GeneralTests"; break;
	case LayoutName::InputTests: return "org.stappler.xenolith.test.InputTests"; break;
	case LayoutName::ActionTests: return "org.stappler.xenolith.test.ActionTests"; break;
	case LayoutName::VgTests: return "org.stappler.xenolith.test.VgTests"; break;
	case LayoutName::GuiTests: return "org.stappler.xenolith.test.GuiTests"; break;
	case LayoutName::Config: return "org.stappler.xenolith.test.Config"; break;

	case LayoutName::GeneralUpdateTest: return "org.stappler.xenolith.test.GeneralUpdateTest"; break;
	case LayoutName::GeneralZOrderTest: return "org.stappler.xenolith.test.GeneralZOrderTest"; break;
	case LayoutName::GeneralLabelTest: return "org.stappler.xenolith.test.GeneralLabelTest"; break;
	case LayoutName::GeneralTransparencyTest: return "org.stappler.xenolith.test.GeneralTransparencyTest"; break;
	case LayoutName::GeneralAutofitTest: return "org.stappler.xenolith.test.GeneralAutofitTest"; break;
	case LayoutName::GeneralTemporaryResourceTest: return "org.stappler.xenolith.test.GeneralTemporaryResourceTest"; break;

	case LayoutName::InputTouchTest: return "org.stappler.xenolith.test.InputTouchTest"; break;
	case LayoutName::InputKeyboardTest: return "org.stappler.xenolith.test.InputKeyboardTest"; break;
	case LayoutName::InputTapPressTest: return "org.stappler.xenolith.test.InputTapPressTest"; break;
	case LayoutName::InputSwipeTest: return "org.stappler.xenolith.test.InputSwipeTest"; break;
	case LayoutName::InputTextTest: return "org.stappler.xenolith.test.InputTextTest"; break;

	case LayoutName::ActionProgressTest: return "org.stappler.xenolith.test.ActionProgressTest"; break;
	case LayoutName::ActionEaseTest: return "org.stappler.xenolith.test.ActionEaseTest"; break;

	case LayoutName::VgTessTest: return "org.stappler.xenolith.test.VgTessTest"; break;
	case LayoutName::VgIconTest: return "org.stappler.xenolith.test.VgIconTest"; break;
	case LayoutName::VgIconList: return "org.stappler.xenolith.test.VgIconList"; break;

	case LayoutName::GuiScrollTest: return "org.stappler.xenolith.test.GuiScrollTest"; break;
	}
	return StringView();
}

LayoutName getLayoutNameById(StringView name) {
	if (name == "org.stappler.xenolith.test.Root") { return LayoutName::Root; }
	else if (name == "org.stappler.xenolith.test.GeneralTests") { return LayoutName::GeneralTests; }
	else if (name == "org.stappler.xenolith.test.InputTests") { return LayoutName::InputTests; }
	else if (name == "org.stappler.xenolith.test.ActionTests") { return LayoutName::ActionTests; }
	else if (name == "org.stappler.xenolith.test.VgTests") { return LayoutName::VgTests; }
	else if (name == "org.stappler.xenolith.test.GuiTests") { return LayoutName::GuiTests; }
	else if (name == "org.stappler.xenolith.test.Config") { return LayoutName::Config; }

	else if (name == "org.stappler.xenolith.test.GeneralUpdateTest") { return LayoutName::GeneralUpdateTest; }
	else if (name == "org.stappler.xenolith.test.GeneralZOrderTest") { return LayoutName::GeneralZOrderTest; }
	else if (name == "org.stappler.xenolith.test.GeneralLabelTest") { return LayoutName::GeneralLabelTest; }
	else if (name == "org.stappler.xenolith.test.GeneralTransparencyTest") { return LayoutName::GeneralTransparencyTest; }
	else if (name == "org.stappler.xenolith.test.GeneralAutofitTest") { return LayoutName::GeneralAutofitTest; }
	else if (name == "org.stappler.xenolith.test.GeneralTemporaryResourceTest") { return LayoutName::GeneralTemporaryResourceTest; }

	else if (name == "org.stappler.xenolith.test.InputTouchTest") { return LayoutName::InputTouchTest; }
	else if (name == "org.stappler.xenolith.test.InputKeyboardTest") { return LayoutName::InputKeyboardTest; }
	else if (name == "org.stappler.xenolith.test.InputTapPressTest") { return LayoutName::InputTapPressTest; }
	else if (name == "org.stappler.xenolith.test.InputSwipeTest") { return LayoutName::InputSwipeTest; }
	else if (name == "org.stappler.xenolith.test.InputTextTest") { return LayoutName::InputTextTest; }

	else if (name == "org.stappler.xenolith.test.ActionProgressTest") { return LayoutName::ActionProgressTest; }
	else if (name == "org.stappler.xenolith.test.ActionEaseTest") { return LayoutName::ActionEaseTest; }

	else if (name == "org.stappler.xenolith.test.VgTessTest") { return LayoutName::VgTessTest; }
	else if (name == "org.stappler.xenolith.test.VgIconTest") { return LayoutName::VgIconTest; }
	else if (name == "org.stappler.xenolith.test.VgIconList") { return LayoutName::VgIconList; }

	else if (name == "org.stappler.xenolith.test.GuiScrollTest") { return LayoutName::GuiScrollTest; }

	return LayoutName::Root;
}

Rc<Node> makeLayoutNode(LayoutName name) {
	switch (name) {
	case LayoutName::Root: return Rc<RootLayout>::create(); break;
	case LayoutName::GeneralTests: return Rc<LayoutMenu>::create(name, s_generalTestsMenu); break;
	case LayoutName::InputTests: return Rc<LayoutMenu>::create(name, s_inputTestsMenu); break;
	case LayoutName::ActionTests: return Rc<LayoutMenu>::create(name, s_actionTestsMenu); break;
	case LayoutName::VgTests: return Rc<LayoutMenu>::create(name, s_vgTestsMenu); break;
	case LayoutName::GuiTests: return Rc<LayoutMenu>::create(name, s_guiTestsMenu); break;
	case LayoutName::Config: return Rc<ConfigMenu>::create(); break;

	case LayoutName::GeneralUpdateTest: return Rc<GeneralUpdateTest>::create(); break;
	case LayoutName::GeneralZOrderTest: return Rc<GeneralZOrderTest>::create(); break;
	case LayoutName::GeneralLabelTest: return Rc<GeneralLabelTest>::create(); break;
	case LayoutName::GeneralTransparencyTest: return Rc<GeneralTransparencyTest>::create(); break;
	case LayoutName::GeneralAutofitTest: return Rc<GeneralAutofitTest>::create(); break;
	case LayoutName::GeneralTemporaryResourceTest: return Rc<GeneralTemporaryResourceTest>::create(); break;

	case LayoutName::InputTouchTest: return Rc<InputTouchTest>::create(); break;
	case LayoutName::InputKeyboardTest: return Rc<InputKeyboardTest>::create(); break;
	case LayoutName::InputTapPressTest: return Rc<InputTapPressTest>::create(); break;
	case LayoutName::InputSwipeTest: return Rc<InputSwipeTest>::create(); break;
	case LayoutName::InputTextTest: return Rc<InputTextTest>::create(); break;

	case LayoutName::ActionProgressTest: return Rc<RootLayout>::create(); break;
	case LayoutName::ActionEaseTest: return Rc<ActionEaseTest>::create(); break;

	case LayoutName::VgTessTest: return Rc<VgTessTest>::create(); break;
	case LayoutName::VgIconTest: return Rc<VgIconTest>::create(); break;
	case LayoutName::VgIconList: return Rc<VgIconList>::create(); break;

	case LayoutName::GuiScrollTest: return Rc<RootLayout>::create(); break;
	}
	return nullptr;
}

}
