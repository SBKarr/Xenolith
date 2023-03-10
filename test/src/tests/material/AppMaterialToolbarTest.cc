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

#include "AppMaterialToolbarTest.h"
#include "MaterialStyleContainer.h"

namespace stappler::xenolith::app {

bool MaterialToolbarTest::init() {
	if (!MaterialTest::init(LayoutName::MaterialToolbarTest, "")) {
		return false;
	}

	_colorHct = material::ColorHCT(Color::Red_500);

	_style = addComponent(Rc<material::StyleContainer>::create());
	_style->setPrimaryScheme(material::ThemeType::LightTheme, _colorHct, false);

	_huePicker = addChild(Rc<MaterialColorPicker>::create(MaterialColorPicker::Hue,_colorHct, [this] (float val) {
		auto newColor = material::ColorHCT(val, _colorHct.data.chroma, _colorHct.data.tone, 1.0f);
		_colorHct = move(newColor);
		_style->setPrimaryScheme(_themeType, _colorHct, false);
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_appBar = setFlexibleNode(Rc<material::AppBar>::create(material::AppBarLayout::Small, material::SurfaceStyle(
			material::NodeStyle::Filled, material::ColorRole::PrimaryContainer)));
	_appBar->setTitle("Test App Bar");
	_appBar->setNavButtonIcon(IconName::Navigation_arrow_back_solid);
	_appBar->setNavCallback([this] {
		if (_scene) {
			((AppScene *)_scene)->runLayout(_layoutRoot, makeLayoutNode(_layoutRoot));
		}
	});

	auto actionMenu = Rc<material::MenuSource>::create();
	actionMenu->addButton("", IconName::Editor_format_align_center_solid, [this] (material::Button *, material::MenuSourceButton *) {
		if (_appBar->getLayout() == material::AppBarLayout::CenterAligned) {
			_appBar->setLayout(material::AppBarLayout::Small);
		} else {
			_appBar->setLayout(material::AppBarLayout::CenterAligned);
		}
	});
	actionMenu->addButton("", IconName::Editor_vertical_align_top_solid, [this] (material::Button *, material::MenuSourceButton *) {
		_director->getView()->setDecorationVisible(!_decorationVisible);
		_decorationVisible = !_decorationVisible;
	});
	_appBar->setActionMenuSource(move(actionMenu));

	_scrollView = setBaseNode(Rc<ScrollView>::create(ScrollView::Vertical));
	_scrollController = _scrollView->setController(Rc<ScrollController>::create());

	for (uint32_t i = 0; i < 36; ++ i) {
		_scrollController->addItem([i] (const ScrollController::Item &item) {
			return Rc<Layer>::create(Color(Color::Tone(i % 12), Color::Level::a200));
		}, 128.0f);
	}

	setFlexibleMinHeight(0.0f);
	setFlexibleMaxHeight(56.0f);

	_backButton->setVisible(false);

	return true;
}

void MaterialToolbarTest::onContentSizeDirty() {
	MaterialTest::onContentSizeDirty();

	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 298.0f - 48.0f - _decorationPadding.horizontal()), 360.0f), 24.0f));
	_huePicker->setPosition(Vec2(32.0f + _decorationPadding.left, _contentSize.height - _decorationPadding.top - 96.0f));
}

}
