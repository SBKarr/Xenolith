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

#include "AppMaterialNodeTest.h"
#include "MaterialLabel.h"

namespace stappler::xenolith::app {

class MaterialNodeWithLabel : public material::MaterialNode {
public:
	virtual ~MaterialNodeWithLabel() { }

	virtual bool init(material::StyleData &&, StringView);
	virtual bool init(const material::StyleData &, StringView);

	virtual void onContentSizeDirty() override;

protected:
	bool initialize(StringView);

	material::MaterialLabel *_label = nullptr;
};

bool MaterialNodeWithLabel::init(material::StyleData &&style, StringView str) {
	if (!MaterialNode::init(move(style))) {
		return false;
	}

	return initialize(str);
}

bool MaterialNodeWithLabel::init(const material::StyleData &style, StringView str) {
	if (!MaterialNode::init(style)) {
		return false;
	}

	return initialize(str);
}

bool MaterialNodeWithLabel::initialize(StringView str) {
	_label = addChild(Rc<material::MaterialLabel>::create(material::TypescaleRole::TitleLarge, str));
	_label->setAnchorPoint(Anchor::Middle);

	return true;
}

void MaterialNodeWithLabel::onContentSizeDirty() {
	MaterialNode::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

bool MaterialNodeTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialNodeTest, "")) {
		return false;
	}

	auto color = material::ColorHCT(Color::Red_500.asColor4F());

	_style = addComponent(Rc<material::StyleContainer>::create());
	_style->setPrimaryScheme(material::ThemeType::LightTheme, color.asColor4F(), false);

	addComponent(Rc<material::MaterialNodeInterior>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level1,
		material::ShapeFamily::RoundedCorners, material::ShapeStyle::None, material::NodeStyle::Text
	}));

	_background = addChild(Rc<material::MaterialNode>::create(material::StyleData::Background), -1);
	_background->setAnchorPoint(Anchor::Middle);

	_nodeElevation = addChild(Rc<MaterialNodeWithLabel>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level1
	}, "Elevation"), 1);
	_nodeElevation->setContentSize(Size2(160.0f, 160.0f));
	_nodeElevation->setAnchorPoint(Anchor::Middle);

	auto el = _nodeElevation->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeElevation->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeElevation->setStyle(move(style), 0.3f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeShadow = addChild(Rc<MaterialNodeWithLabel>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level1,
		material::ShapeFamily::RoundedCorners, material::ShapeStyle::None, material::NodeStyle::TonalElevated
	}, "Shadow"), 1);
	_nodeShadow->setContentSize(Size2(160.0f, 160.0f));
	_nodeShadow->setAnchorPoint(Anchor::Middle);

	el = _nodeShadow->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeShadow->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeShadow->setStyle(move(style), 0.3f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerRounded = addChild(Rc<MaterialNodeWithLabel>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level5,
		material::ShapeFamily::RoundedCorners, material::ShapeStyle::ExtraSmall
	}, "Rounded"), 1);
	_nodeCornerRounded->setContentSize(Size2(160.0f, 160.0f));
	_nodeCornerRounded->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerRounded->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerRounded->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerRounded->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerCut = addChild(Rc<MaterialNodeWithLabel>::create(material::StyleData{
		String(), material::ColorRole::Primary, material::Elevation::Level5,
		material::ShapeFamily::CutCorners, material::ShapeStyle::ExtraSmall
	}, "Cut"), 1);
	_nodeCornerCut->setContentSize(Size2(160.0f, 160.0f));
	_nodeCornerCut->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerCut->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerCut->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerCut->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	_huePicker = addChild(Rc<MaterialColorPickerSprite>::create(MaterialColorPickerSprite::Hue, color, [this] (float val) {
		auto color = material::ColorHCT(val, 100.0f, 50.0f, 1.0f);
		_style->setPrimaryScheme(_lightCheckbox->getValue() ? material::ThemeType::DarkTheme : material::ThemeType::LightTheme, color, false);
		_huePicker->setTargetColor(color);
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_lightCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Dark theme", false, [this] (bool value) {
		if (value) {
			_style->setPrimaryScheme(material::ThemeType::DarkTheme, _huePicker->getTargetColor(), false);
		} else {
			_style->setPrimaryScheme(material::ThemeType::LightTheme, _huePicker->getTargetColor(), false);
		}
	}));
	_lightCheckbox->setAnchorPoint(Anchor::TopLeft);
	_lightCheckbox->setContentSize(Size2(24.0f, 24.0f));

	return true;
}

void MaterialNodeTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
	_nodeElevation->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, 100.0f));
	_nodeShadow->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, 100.0f));
	_nodeCornerRounded->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, -100.0f));
	_nodeCornerCut->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, -100.0f));

	_huePicker->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_lightCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 48.0f));
}

void MaterialNodeTest::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.3f), 1.5f, Color::White);
	auto ambient = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), 1.5f, Color::White);

	_scene->removeAllLights();
	_scene->addLight(move(light));
	_scene->addLight(move(ambient));
}

}
