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

#include "AppMaterialNodeTest.h"

namespace stappler::xenolith::app {

bool MaterialNodeTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialNodeTest, "")) {
		return false;
	}

	_style = addComponent(Rc<material::StyleContainer>::create());
	_style->setPrimaryScheme(material::ThemeType::LightTheme, Color::Red_500.asColor4F(), false);

	_nodeElevation = addChild(Rc<material::MaterialNode>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level1
	}), 1);
	_nodeElevation->setContentSize(Size2(100.0f, 100.0f));
	_nodeElevation->setAnchorPoint(Anchor::Middle);

	auto el = _nodeElevation->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeElevation->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeElevation->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeShadow = addChild(Rc<material::MaterialNode>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level1,
		material::ShapeFamily::RoundedCorners, material::ShapeStyle::None, true
	}), 1);
	_nodeShadow->setContentSize(Size2(100.0f, 100.0f));
	_nodeShadow->setAnchorPoint(Anchor::Middle);

	el = _nodeShadow->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeShadow->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeShadow->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerRounded = addChild(Rc<material::MaterialNode>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level5,
		material::ShapeFamily::RoundedCorners, material::ShapeStyle::ExtraSmall
	}), 1);
	_nodeCornerRounded->setContentSize(Size2(100.0f, 100.0f));
	_nodeCornerRounded->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerRounded->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerRounded->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerRounded->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerCut = addChild(Rc<material::MaterialNode>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level5,
		material::ShapeFamily::CutCorners, material::ShapeStyle::ExtraSmall
	}), 1);
	_nodeCornerCut->setContentSize(Size2(100.0f, 100.0f));
	_nodeCornerCut->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerCut->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerCut->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerCut->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	return true;
}

void MaterialNodeTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_nodeElevation->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, 100.0f));
	_nodeShadow->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, 100.0f));
	_nodeCornerRounded->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, -100.0f));
	_nodeCornerCut->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, -100.0f));
}

}
