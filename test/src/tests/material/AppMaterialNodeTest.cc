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

class MaterialNodeWithLabel : public material::Surface {
public:
	virtual ~MaterialNodeWithLabel() { }

	virtual bool init(const material::SurfaceStyle &, StringView);

	virtual void onContentSizeDirty() override;

protected:
	bool initialize(StringView);

	Label *_label = nullptr;
};

bool MaterialNodeWithLabel::init(const material::SurfaceStyle &style, StringView str) {
	if (!Surface::init(style)) {
		return false;
	}

	return initialize(str);
}

bool MaterialNodeWithLabel::initialize(StringView str) {
	_label = addChild(Rc<material::TypescaleLabel>::create(material::TypescaleRole::TitleLarge, str), 1);
	_label->setAnchorPoint(Anchor::Middle);

	return true;
}

void MaterialNodeWithLabel::onContentSizeDirty() {
	Surface::onContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
}

bool MaterialNodeTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialNodeTest, "")) {
		return false;
	}

	_background = addChild(Rc<MaterialBackground>::create(Color::Red_500), -1);
	_background->setAnchorPoint(Anchor::Middle);

	_nodeElevation = _background->addChild(Rc<MaterialNodeWithLabel>::create(material::SurfaceStyle{
		material::ColorRole::Primary, material::Elevation::Level1
	}, "Elevation"), 1);
	_nodeElevation->setContentSize(Size2(160.0f, 100.0f));
	_nodeElevation->setAnchorPoint(Anchor::Middle);

	auto el = _nodeElevation->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeElevation->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeElevation->setStyle(move(style), 0.3f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeShadow = _background->addChild(Rc<MaterialNodeWithLabel>::create(material::SurfaceStyle{
		material::ColorRole::Primary, material::Elevation::Level1, material::NodeStyle::SurfaceTonalElevated
	}, "Shadow"), 1);
	_nodeShadow->setContentSize(Size2(160.0f, 100.0f));
	_nodeShadow->setAnchorPoint(Anchor::Middle);

	el = _nodeShadow->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeShadow->getStyleTarget();
		style.elevation = material::Elevation((toInt(style.elevation) + 1) % (toInt(material::Elevation::Level5) + 1));
		_nodeShadow->setStyle(move(style), 0.3f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerRounded = _background->addChild(Rc<MaterialNodeWithLabel>::create(material::SurfaceStyle{
		material::Elevation::Level5, material::ShapeFamily::RoundedCorners, material::ShapeStyle::ExtraSmall
	}, "Rounded"), 1);
	_nodeCornerRounded->setContentSize(Size2(160.0f, 100.0f));
	_nodeCornerRounded->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerRounded->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerRounded->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerRounded->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeCornerCut = _background->addChild(Rc<MaterialNodeWithLabel>::create(material::SurfaceStyle{
		material::Elevation::Level5, material::ShapeFamily::CutCorners, material::ShapeStyle::ExtraSmall
	}, "Cut"), 1);
	_nodeCornerCut->setContentSize(Size2(160.0f, 100.0f));
	_nodeCornerCut->setAnchorPoint(Anchor::Middle);

	el = _nodeCornerCut->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		auto style = _nodeCornerCut->getStyleTarget();
		style.shapeStyle = material::ShapeStyle((toInt(style.shapeStyle) + 1) % (toInt(material::ShapeStyle::Full) + 1));
		_nodeCornerCut->setStyle(move(style), 0.25f);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);


	_nodeStyle = _background->addChild(Rc<MaterialNodeWithLabel>::create(material::SurfaceStyle{
		material::Elevation::Level5, material::NodeStyle::Outlined, material::ShapeStyle::Full, material::ActivityState::Enabled
	}, "Style"), 1);
	_nodeStyle->setContentSize(Size2(160.0f, 100.0f));
	_nodeStyle->setAnchorPoint(Anchor::Middle);

	el = _nodeStyle->addInputListener(Rc<InputListener>::create());
	el->addTapRecognizer([this] (GestureTap tap) {
		if (tap.input->data.button == InputMouseButton::MouseLeft) {
			auto style = _nodeStyle->getStyleTarget();
			 style.nodeStyle = material::NodeStyle((toInt(style.nodeStyle) + 1) % (toInt(material::NodeStyle::Text) + 1));
			_nodeStyle->setStyle(move(style), 0.25f);
		} else {
			auto style = _nodeStyle->getStyleTarget();
			style.activityState = material::ActivityState((toInt(style.activityState) + 1) % (toInt(material::ActivityState::Pressed) + 1));
			_nodeStyle->setStyle(move(style), 0.25f);
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft, InputMouseButton::MouseRight}), 1);

	return true;
}

void MaterialNodeTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);
	_nodeElevation->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, 20.0f));
	_nodeShadow->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, 20.0f));
	_nodeCornerRounded->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, -100.0f));
	_nodeCornerCut->setPosition(Vec2(_contentSize / 2.0f) - Vec2(-100.0f, -100.0f));
	_nodeStyle->setPosition(Vec2(_contentSize / 2.0f) - Vec2(100.0f, 140.0f));
}

}
