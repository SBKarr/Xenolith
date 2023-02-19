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

#include "AppGeneralShadowTest.h"

namespace stappler::xenolith::app {

class LightNormalSelectorPoint : public VectorSprite {
public:
	virtual ~LightNormalSelectorPoint() { }

	virtual bool init() override;

protected:
	using VectorSprite::init;
};

class GeneralShadowTest::LightNormalSelector : public VectorSprite {
public:
	virtual ~LightNormalSelector() { }

	virtual bool init() override;
	virtual void onContentSizeDirty() override;
	virtual void onEnter(Scene *) override;

	void setSoftShadow(bool);
	void setK(float);
	void setAmbient(bool);

protected:
	using VectorSprite::init;

	void updateLightNormal(const Vec2 &);

	LightNormalSelectorPoint *_point = nullptr;
	Vec2 _normal = Vec2::ZERO;
	float _k = 1.0f;
	bool _softShadow = true;
	bool _ambient = false;
};

bool LightNormalSelectorPoint::init() {
	auto image = Rc<VectorImage>::create(Size2(10, 10));
	image->addPath("", "org.stappler.xenolith.tess.TessPoint")
		->setFillColor(Color::White)
		.addOval(Rect(0, 0, 10, 10))
		.setAntialiased(false);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	return true;
}

bool GeneralShadowTest::LightNormalSelector::init() {
	auto image = Rc<VectorImage>::create(Size2(16.0f, 16.0f));
	image->addPath(VectorPath()
			.addCircle(8.0f, 8.0f, 7.0f)
			.setStyle(VectorPath::DrawStyle::Stroke)
			.setStrokeWidth(1.0f)
			.setStrokeColor(Color::Grey_500));

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	_point = addChild(Rc<LightNormalSelectorPoint>::create());
	_point->setLocalZOrder(1);
	_point->setColor(Color::Red_500);
	_point->setAnchorPoint(Anchor::Middle);

	auto l = addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (const GestureData &data) -> bool {
		if (data.event == GestureEvent::Moved || data.event == GestureEvent::Ended) {
			auto maxDist = (_contentSize.width + _contentSize.height) / 5.0f;
			auto pos = convertToNodeSpace(data.input->currentLocation);
			auto dist = pos.distance(Vec2(_contentSize / 2.0f));
			if (dist < maxDist) {
				_point->setPosition(pos);
			} else {
				_point->setPosition(Vec2(_contentSize / 2.0f) + (pos - Vec2(_contentSize / 2.0f)).getNormalized() * maxDist);
			}

			_normal = (_point->getPosition().xy() - Vec2(_contentSize / 2.0f)) / (maxDist * 3.0f);
			updateLightNormal(_normal);
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft}));

	return true;
}

void GeneralShadowTest::LightNormalSelector::onContentSizeDirty() {
	VectorSprite::onContentSizeDirty();

	_point->setPosition(Vec2(_contentSize / 2.0f) + _normal * Vec2(_contentSize / 2.0f));
}

void GeneralShadowTest::LightNormalSelector::onEnter(Scene *scene) {
	VectorSprite::onEnter(scene);

	updateLightNormal(_normal);
}

void GeneralShadowTest::LightNormalSelector::setSoftShadow(bool soft) {
	_softShadow = soft;
	updateLightNormal(_normal);
}

void GeneralShadowTest::LightNormalSelector::setK(float k) {
	_k = k;
	updateLightNormal(_normal);
}

void GeneralShadowTest::LightNormalSelector::setAmbient(bool value) {
	_ambient = value;
	updateLightNormal(_normal);
}

void GeneralShadowTest::LightNormalSelector::updateLightNormal(const Vec2 &vec) {
	if (!_scene) {
		return;
	}

	auto light = Rc<SceneLight>::create(SceneLightType::Ambient, vec, _k, Color::White);
	light->setSoftShadow(_softShadow);

	_scene->removeAllLights();
	_scene->addLight(move(light));

	if (_ambient) {
		auto ambient = Rc<SceneLight>::create(SceneLightType::Ambient, Vec2(0.0f, 0.0f), _k, Color::White);

		_scene->addLight(move(ambient));
	}
}

bool GeneralShadowTest::init() {
	if (!LayoutTest::init(LayoutName::GeneralShadowTest, "")) {
		return false;
	}

	float maxShadow = 40.0f;
	float initialShadow = 4.0f;
	float initialScale = 1.0f;
	float initialK = 1.5f;

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_sprite = addChild(Rc<VectorSprite>::create(move(image)), 0);
		_sprite->setContentSize(Size2(256, 256));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setColor(Color::Grey_100);
		_sprite->setQuality(0.75f);
		_sprite->setScale(initialScale);
		_sprite->setShadowIndex(initialShadow);
	} while (0);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(32);
	_label->setString(getIconName(_currentName));
	_label->setAnchorPoint(Anchor::MiddleTop);

	_info = addChild(Rc<Label>::create());
	_info->setFontSize(24);
	_info->setString("Test");
	_info->setAnchorPoint(Anchor::MiddleTop);

	_sliderScale = addChild(Rc<AppSliderWithLabel>::create(toString("Scale: ", initialScale),
			(initialScale - 0.1f) / 2.9f, [this] (float val) {
		updateScaleValue(val);
	}));
	_sliderScale->setAnchorPoint(Anchor::TopLeft);
	_sliderScale->setContentSize(Size2(128.0f, 32.0f));

	_sliderShadow = addChild(Rc<AppSliderWithLabel>::create(toString("Shadow: ", initialShadow),
			initialShadow / maxShadow, [this, maxShadow] (float val) {
		_sprite->setShadowIndex(val * maxShadow);
		_sliderShadow->setString(toString("Shadow: ", _sprite->getShadowIndex()));
	}));
	_sliderShadow->setAnchorPoint(Anchor::TopLeft);
	_sliderShadow->setContentSize(Size2(128.0f, 32.0f));

	_sliderK = addChild(Rc<AppSliderWithLabel>::create(toString("K: ", initialK),
			initialK / 2.0f, [this] (float val) {
		_normalSelector->setK(val * 2.0f);
		_sliderK->setString(toString("K: ", val * 2.0f));
	}));
	_sliderK->setAnchorPoint(Anchor::TopLeft);
	_sliderK->setContentSize(Size2(128.0f, 32.0f));

	_normalSelector = addChild(Rc<LightNormalSelector>::create());
	_normalSelector->setAnchorPoint(Anchor::TopLeft);
	_normalSelector->setContentSize(Size2(92.0f, 92.0f));
	_normalSelector->setK(initialK);

	_checkboxAmbient = addChild(Rc<AppCheckboxWithLabel>::create("Ambient", false, [this] (bool value) {
		_normalSelector->setAmbient(value);
	}));
	_checkboxAmbient->setAnchorPoint(Anchor::TopLeft);
	_checkboxAmbient->setContentSize(Size2(32.0f, 32.0f));

	auto l = _sprite->addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (const GestureData &data) -> bool {
		if (data.event == GestureEvent::Ended) {
			if (data.input->data.button == InputMouseButton::Mouse8 || data.input->data.button == InputMouseButton::MouseScrollRight
					|| data.input->data.button == InputMouseButton::MouseLeft) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (data.input->data.button == InputMouseButton::Mouse9 || data.input->data.button == InputMouseButton::MouseScrollLeft
					|| data.input->data.button == InputMouseButton::MouseRight) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft, InputMouseButton::MouseRight,
		InputMouseButton::MouseScrollLeft, InputMouseButton::MouseScrollRight, InputMouseButton::Mouse8, InputMouseButton::Mouse9}));

	l->addKeyRecognizer([this] (const GestureData &ev) {
		if (ev.event == GestureEvent::Ended) {
			if (ev.input->data.key.keycode == InputKeyCode::LEFT) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (ev.input->data.key.keycode == InputKeyCode::RIGHT) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeKeyMask({InputKeyCode::LEFT, InputKeyCode::RIGHT}));

	scheduleUpdate();
	updateIcon(_currentName);

	return true;
}

void GeneralShadowTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_sprite->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));

	_label->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 198.0f));
	_info->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 240.0f));

	_sliderScale->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_sliderShadow->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));
	_sliderK->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f * 2.0f));
	_checkboxAmbient->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f * 3.0f));
	_normalSelector->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f * 4.0f));
}

void GeneralShadowTest::update(const UpdateTime &t) {
	LayoutTest::update(t);

	uint32_t ntriangles = _sprite->getTrianglesCount();
	uint32_t nvertexes = _sprite->getVertexesCount();

	_info->setString(toString("V: ", nvertexes, "; T: ", ntriangles));
}

void GeneralShadowTest::setDataValue(Value &&data) {
	if (data.isInteger("icon")) {
		auto icon = IconName(data.getInteger("icon"));
		if (icon != _currentName) {
			updateIcon(icon);
			return;
		}
	}

	LayoutTest::setDataValue(move(data));
}

void GeneralShadowTest::updateIcon(IconName name) {
	_currentName = name;
	_label->setString(toString(getIconName(_currentName), " ", toInt(_currentName), "/", toInt(IconName::Toggle_toggle_on_solid)));

	_sprite->clear();
	auto path = _sprite->addPath();
	getIconData(_currentName, [&] (BytesView bytes) {
		path->getPath()->init(bytes);
	});
	path->setWindingRule(vg::Winding::EvenOdd);
	path->setAntialiased(false);
	auto t = Mat4::IDENTITY;
	t.scale(1, -1, 1);
	t.translate(0, -24, 0);
	path->setTransform(t);

	LayoutTest::setDataValue(Value({
		pair("icon", Value(toInt(_currentName)))
	}));
}

void GeneralShadowTest::updateScaleValue(float value) {
	auto q = 0.1f + 2.9f * value;

	_sliderScale->setString(toString("Scale: ", q));
	_sprite->setScale(q);
}

}
