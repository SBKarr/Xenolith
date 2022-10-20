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

#include "AppVgIconTest.h"
#include "XLGuiLayerRounded.h"

namespace stappler::xenolith::app {

bool VgIconTest::init() {
	if (!LayoutTest::init(LayoutName::VgIconTest, "")) {
		return false;
	}

	float initialQuality = 2.0f;
	float initialScale = 0.5f;

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_sprite = addChild(Rc<VectorSprite>::create(move(image)), 0);
		_sprite->setContentSize(Size2(256, 256));
		_sprite->setAnchorPoint(Anchor::Middle);
		_sprite->setColor(Color::Black);
		_sprite->setOpacity(0.5f);
		_sprite->setQuality(initialQuality);
		_sprite->setScale(initialScale);
	} while (0);

	do {
		auto image = Rc<VectorImage>::create(Size2(24, 24));

		_triangles = addChild(Rc<VectorSprite>::create(move(image)), 1);
		_triangles->setContentSize(Size2(256, 256));
		_triangles->setAnchorPoint(Anchor::Middle);
		_triangles->setColor(Color::Green_500);
		_triangles->setOpacity(0.5f);
		_triangles->setLineWidth(1.0f);
		_triangles->setQuality(initialQuality);
		_triangles->setVisible(false);
		_triangles->setScale(initialScale);
	} while (0);

	_spriteLayer = addChild(Rc<LayerRounded>::create(Color::Grey_100, 20.0f), -1);
	_spriteLayer->setContentSize(Size2(256, 256));
	_spriteLayer->setAnchorPoint(Anchor::Middle);

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(32);
	_label->setString(getIconName(_currentName));
	_label->setAnchorPoint(Anchor::MiddleTop);

	_info = addChild(Rc<Label>::create());
	_info->setFontSize(24);
	_info->setString("Test");
	_info->setAnchorPoint(Anchor::MiddleTop);

	_sliderQuality = addChild(Rc<AppSliderWithLabel>::create(toString("Quality: ", initialQuality),
			(initialQuality - 0.1f) / 4.9f, [this] (float val) {
		updateQualityValue(val);
	}));
	_sliderQuality->setAnchorPoint(Anchor::TopLeft);
	_sliderQuality->setContentSize(Size2(128.0f, 32.0f));

	_sliderScale = addChild(Rc<AppSliderWithLabel>::create(toString("Scale: ", initialScale),
			(initialScale - 0.1f) / 2.9f, [this] (float val) {
		updateScaleValue(val);
	}));
	_sliderScale->setAnchorPoint(Anchor::TopLeft);
	_sliderScale->setContentSize(Size2(128.0f, 32.0f));

	_checkboxVisible = addChild(Rc<AppCheckboxWithLabel>::create("Triangles", false, [this] (bool value) {
		_triangles->setVisible(value);
	}));
	_checkboxVisible->setAnchorPoint(Anchor::TopLeft);
	_checkboxVisible->setContentSize(Size2(32.0f, 32.0f));

	_checkboxAntialias = addChild(Rc<AppCheckboxWithLabel>::create("Antialias", _antialias, [this] (bool value) {
		updateAntialiasValue(value);
	}));
	_checkboxAntialias->setAnchorPoint(Anchor::TopLeft);
	_checkboxAntialias->setContentSize(Size2(32.0f, 32.0f));

	auto l = _sprite->addInputListener(Rc<InputListener>::create());
	l->addTouchRecognizer([this] (const GestureData &data) -> bool {
		if (data.event == GestureEvent::Ended) {
			if (data.input->data.button == InputMouseButton::Mouse8) {
				if (_currentName == IconName::Action_3d_rotation_outline) {
					updateIcon(IconName::Toggle_toggle_on_solid);
				} else {
					updateIcon(IconName(toInt(_currentName) - 1));
				}
			} else if (data.input->data.button == InputMouseButton::Mouse9) {
				if (_currentName == IconName::Toggle_toggle_on_solid) {
					updateIcon(IconName::Action_3d_rotation_outline);
				} else {
					updateIcon(IconName(toInt(_currentName) + 1));
				}
			}
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseScrollLeft, InputMouseButton::MouseScrollRight, InputMouseButton::Mouse8, InputMouseButton::Mouse9}));

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

void VgIconTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_sprite->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_triangles->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));
	_spriteLayer->setPosition(Size2(_contentSize.width / 2.0f, _contentSize.height / 2.0f));

	_label->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 128.0f));
	_info->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, 180.0f));

	_sliderQuality->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));
	_sliderScale->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 48.0f));

	_checkboxVisible->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 96.0f));
	_checkboxAntialias->setPosition(Vec2(16.0f, _contentSize.height - 16.0f - 144.0f));
}

void VgIconTest::update(const UpdateTime &t) {
	LayoutTest::update(t);

	uint32_t ntriangles = _sprite->getTrianglesCount();
	uint32_t nvertexes = _sprite->getVertexesCount();

	_info->setString(toString("V: ", nvertexes, "; T: ", ntriangles));
}

void VgIconTest::setDataValue(Value &&data) {
	if (data.isInteger("icon")) {
		auto icon = IconName(data.getInteger("icon"));
		if (icon != _currentName) {
			updateIcon(icon);
			return;
		}
	}

	LayoutTest::setDataValue(move(data));
}

void VgIconTest::updateIcon(IconName name) {
	_currentName = name;
	_label->setString(toString(getIconName(_currentName), " ", toInt(_currentName), "/", toInt(IconName::Toggle_toggle_on_solid)));

	StringView pathData(
			"M13 13v8h8v-8h-8z"
			"M3 21h8v-8H3v8z"
			"M3 3v8h8V3H3z"
			"M16.6599998 1.69000006 L11 7.34 16.66 13l5.66-5.66-5.66-5.65z"
	);

	do {
		_sprite->clear();
		auto path = _sprite->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		path->setAntialiased(_antialias);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);

	do {
		_triangles->clear();
		auto path = _triangles->addPath();
		//path->getPath()->init(pathData);
		getIconData(_currentName, [&] (BytesView bytes) {
			path->getPath()->init(bytes);
		});
		path->setWindingRule(vg::Winding::EvenOdd);
		path->setAntialiased(false);
		auto t = Mat4::IDENTITY;
		t.scale(1, -1, 1);
		t.translate(0, -24, 0);
		path->setTransform(t);
	} while (0);

	LayoutTest::setDataValue(Value({
		pair("icon", Value(toInt(_currentName)))
	}));
}

void VgIconTest::updateQualityValue(float value) {
	auto q = 0.1f + 4.9f * value;

	_sliderQuality->setString(toString("Quality: ", q));
	_sprite->setQuality(q);
	_triangles->setQuality(q);
}

void VgIconTest::updateScaleValue(float value) {
	auto q = 0.1f + 2.9f * value;

	_sliderScale->setString(toString("Scale: ", q));
	_sprite->setScale(q);
	_triangles->setScale(q);
}

void VgIconTest::updateAntialiasValue(bool value) {
	if (_antialias != value) {
		_antialias = value;
		updateIcon(_currentName);
	}
}

}
