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

#include "AppMaterialColorPickerTest.h"

namespace stappler::xenolith::app {

bool MaterialColorSchemeNode::init(material::ColorScheme::Name name) {
	if (!Layer::init()) {
		return false;
	}

	_name = name;
	_labelName = addChild(Rc<Label>::create(), 1);
	_labelName->setFontSize(14);
	_labelName->setAnchorPoint(Anchor::TopLeft);
	_labelName->setOpacity(1.0f);

	_labelDesc = addChild(Rc<Label>::create(), 1);
	_labelDesc->setFontSize(14);
	_labelDesc->setAnchorPoint(Anchor::BottomRight);
	_labelDesc->setOpacity(1.0f);

	return true;
}

void MaterialColorSchemeNode::onContentSizeDirty() {
	Layer::onContentSizeDirty();
	_labelName->setPosition(Vec2(4.0f, _contentSize.height - 2.0f));
	_labelDesc->setPosition(Vec2(_contentSize.width - 4.0f, 4.0f));
}

void MaterialColorSchemeNode::setSchemeColor(material::ColorScheme::Type type, Color4B background, Color4B label) {
	setColor(Color4F(background));
	_labelName->setColor(Color4F(label));
	_labelDesc->setColor(Color4F(label));
	_type = type;
	updateLabels();
}

void MaterialColorSchemeNode::updateLabels() {
	switch (_type) {
	case material::ColorScheme::LightTheme:
		switch (_name) {
		case material::ColorScheme::Primary: _labelName->setString("Primary"); _labelDesc->setString("Primary40"); break;
		case material::ColorScheme::OnPrimary: _labelName->setString("On Primary"); _labelDesc->setString("Primary100"); break;
		case material::ColorScheme::PrimaryContainer: _labelName->setString("Primary Container"); _labelDesc->setString("Primary90"); break;
		case material::ColorScheme::OnPrimaryContainer: _labelName->setString("On Primary Container"); _labelDesc->setString("Primary10"); break;
		case material::ColorScheme::Secondary: _labelName->setString("Secondary"); _labelDesc->setString("Secondary40"); break;
		case material::ColorScheme::OnSecondary: _labelName->setString("On Secondary"); _labelDesc->setString("Secondary100"); break;
		case material::ColorScheme::SecondaryContainer: _labelName->setString("Secondary Container"); _labelDesc->setString("Secondary90"); break;
		case material::ColorScheme::OnSecondaryContainer: _labelName->setString("On Secondary Container"); _labelDesc->setString("Secondary10"); break;
		case material::ColorScheme::Tertiary: _labelName->setString("Tertiary"); _labelDesc->setString("Tertiary40"); break;
		case material::ColorScheme::OnTertiary: _labelName->setString("On Tertiary"); _labelDesc->setString("Tertiary100"); break;
		case material::ColorScheme::TertiaryContainer: _labelName->setString("Tertiary Container"); _labelDesc->setString("Tertiary90"); break;
		case material::ColorScheme::OnTertiaryContainer: _labelName->setString("On Tertiary Container"); _labelDesc->setString("Tertiary10"); break;
		case material::ColorScheme::Error: _labelName->setString("Error"); _labelDesc->setString("Error40"); break;
		case material::ColorScheme::OnError: _labelName->setString("On Error"); _labelDesc->setString("Error100"); break;
		case material::ColorScheme::ErrorContainer: _labelName->setString("Error Container"); _labelDesc->setString("Error90"); break;
		case material::ColorScheme::OnErrorContainer: _labelName->setString("On Error Container"); _labelDesc->setString("Error10"); break;
		case material::ColorScheme::Background: _labelName->setString("Background"); _labelDesc->setString("Neutral99"); break;
		case material::ColorScheme::OnBackground: _labelName->setString("On Background"); _labelDesc->setString("Neutral10"); break;
		case material::ColorScheme::Surface: _labelName->setString("Surface"); _labelDesc->setString("Neutral99"); break;
		case material::ColorScheme::OnSurface: _labelName->setString("On Surface"); _labelDesc->setString("Neutral10"); break;
		case material::ColorScheme::SurfaceVariant: _labelName->setString("Surface Variant"); _labelDesc->setString("Neutral-variant90"); break;
		case material::ColorScheme::OnSurfaceVariant: _labelName->setString("On Surface Variant"); _labelDesc->setString("Neutral-variant30"); break;
		case material::ColorScheme::Outline: _labelName->setString("Outline"); _labelDesc->setString("Neutral-variant50"); break;
		case material::ColorScheme::OutlineVariant: _labelName->setString("Outline Variant"); _labelDesc->setString(""); break;
		case material::ColorScheme::Shadow: _labelName->setString("Shadow"); _labelDesc->setString(""); break;
		case material::ColorScheme::Scrim: _labelName->setString("Scrim"); _labelDesc->setString(""); break;
		case material::ColorScheme::InverseSurface: _labelName->setString("Inverse Surface"); _labelDesc->setString(""); break;
		case material::ColorScheme::InverseOnSurface: _labelName->setString("Inverse On Surface"); _labelDesc->setString(""); break;
		case material::ColorScheme::InversePrimary: _labelName->setString("Inverse Primary"); _labelDesc->setString(""); break;
		case material::ColorScheme::Max: break;
		}
		break;
	case material::ColorScheme::DarkTheme:
		switch (_name) {
		case material::ColorScheme::Primary: _labelName->setString("Primary"); _labelDesc->setString("Primary80"); break;
		case material::ColorScheme::OnPrimary: _labelName->setString("On Primary"); _labelDesc->setString("Primary20"); break;
		case material::ColorScheme::PrimaryContainer: _labelName->setString("Primary Container"); _labelDesc->setString("Primary30"); break;
		case material::ColorScheme::OnPrimaryContainer: _labelName->setString("On Primary Container"); _labelDesc->setString("Primary90"); break;
		case material::ColorScheme::Secondary: _labelName->setString("Secondary"); _labelDesc->setString("Secondary80"); break;
		case material::ColorScheme::OnSecondary: _labelName->setString("On Secondary"); _labelDesc->setString("Secondary20"); break;
		case material::ColorScheme::SecondaryContainer: _labelName->setString("Secondary Container"); _labelDesc->setString("Secondary30"); break;
		case material::ColorScheme::OnSecondaryContainer: _labelName->setString("On Secondary Container"); _labelDesc->setString("Secondary90"); break;
		case material::ColorScheme::Tertiary: _labelName->setString("Tertiary"); _labelDesc->setString("Tertiary80"); break;
		case material::ColorScheme::OnTertiary: _labelName->setString("On Tertiary"); _labelDesc->setString("Tertiary20"); break;
		case material::ColorScheme::TertiaryContainer: _labelName->setString("Tertiary Container"); _labelDesc->setString("Tertiary30"); break;
		case material::ColorScheme::OnTertiaryContainer: _labelName->setString("On Tertiary Container"); _labelDesc->setString("Tertiary90"); break;
		case material::ColorScheme::Error: _labelName->setString("Error"); _labelDesc->setString("Error80"); break;
		case material::ColorScheme::OnError: _labelName->setString("On Error"); _labelDesc->setString("Error20"); break;
		case material::ColorScheme::ErrorContainer: _labelName->setString("Error Container"); _labelDesc->setString("Error30"); break;
		case material::ColorScheme::OnErrorContainer: _labelName->setString("On Error Container"); _labelDesc->setString("Error90"); break;
		case material::ColorScheme::Background: _labelName->setString("Background"); _labelDesc->setString("Neutral10"); break;
		case material::ColorScheme::OnBackground: _labelName->setString("On Background"); _labelDesc->setString("Neutral90"); break;
		case material::ColorScheme::Surface: _labelName->setString("Surface"); _labelDesc->setString("Neutral10"); break;
		case material::ColorScheme::OnSurface: _labelName->setString("On Surface"); _labelDesc->setString("Neutral90"); break;
		case material::ColorScheme::SurfaceVariant: _labelName->setString("Surface Variant"); _labelDesc->setString("Neutral-variant30"); break;
		case material::ColorScheme::OnSurfaceVariant: _labelName->setString("On Surface Variant"); _labelDesc->setString("Neutral-variant80"); break;
		case material::ColorScheme::Outline: _labelName->setString("Outline"); _labelDesc->setString("Neutral-variant60"); break;
		case material::ColorScheme::OutlineVariant: _labelName->setString("Outline Variant"); _labelDesc->setString(""); break;
		case material::ColorScheme::Shadow: _labelName->setString("Shadow"); _labelDesc->setString(""); break;
		case material::ColorScheme::Scrim: _labelName->setString("Scrim"); _labelDesc->setString(""); break;
		case material::ColorScheme::InverseSurface: _labelName->setString("Inverse Surface"); _labelDesc->setString(""); break;
		case material::ColorScheme::InverseOnSurface: _labelName->setString("Inverse On Surface"); _labelDesc->setString(""); break;
		case material::ColorScheme::InversePrimary: _labelName->setString("Inverse Primary"); _labelDesc->setString(""); break;
		case material::ColorScheme::Max: break;
		}
		break;
	}
}

bool MaterialColorPickerSprite::init(Type type, const material::ColorHCT &color, Function<void(float)> &&cb) {
	if (!Sprite::init()) {
		return false;
	}

	_type = type;
	_targetColor = color;
	_callback = move(cb);
	switch (_type) {
	case Type::Hue: _value = _targetColor.data.hue / 360.0f; break;
	case Type::Chroma: _value = _targetColor.data.chroma / 100.0f; break;
	case Type::Tone: _value = _targetColor.data.tone / 100.0f; break;
	}

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(20);
	_label->setAnchorPoint(Anchor::MiddleLeft);
	_label->setString(makeString());

	_indicator = addChild(Rc<Layer>::create(Color::Grey_500));
	_indicator->setAnchorPoint(Anchor::MiddleLeft);

	_input = addInputListener(Rc<InputListener>::create());
	_input->addTouchRecognizer([this] (const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
		case GestureEvent::Activated:
			setValue(math::clamp(convertToNodeSpace(data.input->currentLocation).x / _contentSize.width, 0.0f, 1.0f));
			break;
		default:
			break;
		}
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}));

	return true;
}

void MaterialColorPickerSprite::onContentSizeDirty() {
	Sprite::onContentSizeDirty();

	_label->setPosition(Vec2(_contentSize.width + 16.0f, _contentSize.height / 2.0f));
	_indicator->setContentSize(Size2(2.0f, _contentSize.height + 12.0f));
	_indicator->setPosition(Vec2(_contentSize.width * _value, _contentSize.height / 2.0f));
}

const material::ColorHCT &MaterialColorPickerSprite::getTargetColor() const {
	return _targetColor;
}

void MaterialColorPickerSprite::setTargetColor(const material::ColorHCT &color) {
	if (_targetColor != color) {
		_targetColor = color;
		_vertexesDirty = true;
		_label->setString(makeString());
	}
}

void MaterialColorPickerSprite::setValue(float value) {
	if (_value != value) {
		_value = value;
		updateValue();
		if (_callback) {
			switch (_type) {
			case Type::Hue: _callback(_value * 360.0f); break;
			case Type::Chroma: _callback(_value * 100.0f); break;
			case Type::Tone: _callback(_value * 100.0f); break;
			}
		}
	}
}

float MaterialColorPickerSprite::getValue() const {
	return _value;
}

void MaterialColorPickerSprite::setLabelColor(const Color4F &color) {
	_label->setColor(color);
}

void MaterialColorPickerSprite::updateVertexesColor() {
	_indicator->setPosition(Vec2(_contentSize.width * _value, _contentSize.height / 2.0f));
}

void MaterialColorPickerSprite::initVertexes() {
	_vertexes.init(QuadsCount * 4, QuadsCount * 6);
	_vertexesDirty = true;
}

void MaterialColorPickerSprite::updateVertexes() {
	_vertexes.clear();

	auto texExtent = _texture->getExtent();
	auto texSize = Size2(texExtent.width, texExtent.height);

	texSize = Size2(texSize.width * _textureRect.size.width, texSize.height * _textureRect.size.height);

	Size2 size(_contentSize.width / QuadsCount, _contentSize.height);
	Vec2 origin(0, 0);
	for (size_t i = 0; i < QuadsCount; ++ i) {
		material::ColorHCT color1;
		material::ColorHCT color2;

		switch (_type) {
		case Type::Hue:
			color1 = material::ColorHCT(i * (float(360) / float(QuadsCount)), _targetColor.data.chroma, _targetColor.data.tone, 1.0f);
			color2 = material::ColorHCT((i + 1) * (float(360) / float(QuadsCount)), _targetColor.data.chroma, _targetColor.data.tone, 1.0f);
			break;
		case Type::Chroma:
			color1 = material::ColorHCT(_targetColor.data.hue, i * (float(100) / float(QuadsCount)), _targetColor.data.tone, 1.0f);
			color2 = material::ColorHCT(_targetColor.data.hue, (i + 1) * (float(100) / float(QuadsCount)), _targetColor.data.tone, 1.0f);
			break;
		case Type::Tone:
			color1 = material::ColorHCT(_targetColor.data.hue, _targetColor.data.chroma, i * (float(100) / float(QuadsCount)), 1.0f);
			color2 = material::ColorHCT(_targetColor.data.hue, _targetColor.data.chroma, (i + 1) * (float(100) / float(QuadsCount)), 1.0f);
			break;
		}

		_vertexes.addQuad()
			.setGeometry(Vec4(origin, 0.0f, 1.0f), size)
			.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
			.setColor({color1.asColor4F(), color1.asColor4F(), color2.asColor4F(), color2.asColor4F()});

		origin += Vec2(size.width, 0.0f);
	}

	_vertexColorDirty = false;
}

String MaterialColorPickerSprite::makeString() {
	switch (_type) {
	case Type::Hue:
		return toString("Hue: ", _targetColor.data.hue);
		break;
	case Type::Chroma:
		return toString("Chroma: ", _targetColor.data.chroma);
		break;
	case Type::Tone:
		return toString("Tone: ", _targetColor.data.tone);
		break;
	}
	return String();
}

void MaterialColorPickerSprite::updateValue() {
	_indicator->setPosition(Vec2(_contentSize.width * _value, _contentSize.height / 2.0f));
}

bool MaterialColorPickerTest::init() {
	if (!LayoutTest::init(LayoutName::MaterialColorPickerTest, "")) {
		return false;
	}

	_colorHct = material::ColorHCT(Color::Purple_500);

	_background = addChild(Rc<Layer>::create(Color::Black), -2);
	_background->setAnchorPoint(Anchor::Middle);
	_background->setVisible(false);

	_lightCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Dark theme", false, [this] (bool value) {
		if (value) {
			_themeType = material::ColorScheme::DarkTheme;
			_background->setVisible(true);
		} else {
			_themeType = material::ColorScheme::LightTheme;
			_background->setVisible(false);
		}
		updateColor(material::ColorHCT(_colorHct));
	}));
	_lightCheckbox->setAnchorPoint(Anchor::TopLeft);
	_lightCheckbox->setContentSize(Size2(24.0f, 24.0f));

	_contentCheckbox = addChild(Rc<AppCheckboxWithLabel>::create("Content theme", false, [this] (bool value) {
		_isContentColor = value;
		updateColor(material::ColorHCT(_colorHct));
	}));
	_contentCheckbox->setAnchorPoint(Anchor::TopLeft);
	_contentCheckbox->setContentSize(Size2(24.0f, 24.0f));

	_huePicker = addChild(Rc<MaterialColorPickerSprite>::create(MaterialColorPickerSprite::Hue,_colorHct, [this] (float val) {
		updateColor(material::ColorHCT(val, _colorHct.data.chroma, _colorHct.data.tone, 1.0f));
	}));
	_huePicker->setAnchorPoint(Anchor::TopLeft);
	_huePicker->setContentSize(Size2(240.0f, 24.0f));

	_chromaPicker = addChild(Rc<MaterialColorPickerSprite>::create(MaterialColorPickerSprite::Chroma,_colorHct, [this] (float val) {
		updateColor(material::ColorHCT(_colorHct.data.hue, val, _colorHct.data.tone, 1.0f));
	}));
	_chromaPicker->setAnchorPoint(Anchor::TopLeft);
	_chromaPicker->setContentSize(Size2(240.0f, 24.0f));

	_tonePicker = addChild(Rc<MaterialColorPickerSprite>::create(MaterialColorPickerSprite::Tone,_colorHct, [this] (float val) {
		updateColor(material::ColorHCT(_colorHct.data.hue, _colorHct.data.chroma, val, 1.0f));
	}));
	_tonePicker->setAnchorPoint(Anchor::TopLeft);
	_tonePicker->setContentSize(Size2(240.0f, 24.0f));

	_spriteLayer = addChild(Rc<LayerRounded>::create(_colorHct, 20.0f), -1);
	_spriteLayer->setContentSize(Size2(98, 98));
	_spriteLayer->setAnchorPoint(Anchor::TopLeft);

	for (auto i = toInt(material::ColorScheme::Primary); i < toInt(material::ColorScheme::Max); ++ i) {
		auto v = addChild(Rc<MaterialColorSchemeNode>::create(material::ColorScheme::Name(i)));
		v->setAnchorPoint(Anchor::TopLeft);
		_nodes[i] = v;
	}

	updateColor(material::ColorHCT(_colorHct));

	return true;
}

void MaterialColorPickerTest::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);
}

void MaterialColorPickerTest::onExit() {
	LayoutTest::onExit();
}

void MaterialColorPickerTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setContentSize(_contentSize);
	_background->setPosition(_contentSize / 2.0f);

	_lightCheckbox->setPosition(Vec2(16.0f, _contentSize.height - 128.0f));
	_contentCheckbox->setPosition(Vec2(240.0f, _contentSize.height - 128.0f));

	_huePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_chromaPicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));
	_tonePicker->setContentSize(Size2(std::min(std::max(160.0f, _contentSize.width - 200.0f - 98.0f - 48.0f), 360.0f), 24.0f));

	_huePicker->setPosition(Vec2(_spriteLayer->getContentSize().width + 32.0f, _contentSize.height - 16.0f));
	_chromaPicker->setPosition(Vec2(_spriteLayer->getContentSize().width + 32.0f, _contentSize.height - 16.0f - 36.0f));
	_tonePicker->setPosition(Vec2(_spriteLayer->getContentSize().width + 32.0f, _contentSize.height - 16.0f - 72.0f));
	_spriteLayer->setPosition(Vec2(16.0f, _contentSize.height - 16.0f));

	Vec2 origin(16.0f, _contentSize.height - 164.0f);
	Size2 size((_contentSize.width - 32.0f) / 4.0f, 48.0f);

	for (auto i = toInt(material::ColorScheme::Primary); i < toInt(material::ColorScheme::Max); ++ i) {
		auto row = i % 4;

		_nodes[i]->setContentSize(size);
		_nodes[i]->setPosition(origin + Vec2(row * size.width, 0.0f));

		if (row == 3) {
			origin.y -= size.height + 4.0f;
		}
	}
}

void MaterialColorPickerTest::updateColor(material::ColorHCT &&color) {
	_colorHct = move(color);
	_spriteLayer->setColor(_colorHct);
	_huePicker->setTargetColor(_colorHct);
	_chromaPicker->setTargetColor(_colorHct);
	_tonePicker->setTargetColor(_colorHct);

	_colorScheme = material::ColorScheme(_themeType, _colorHct, _isContentColor);

	for (auto i = toInt(material::ColorScheme::Primary); i < toInt(material::ColorScheme::Max); ++ i) {
		_nodes[i]->setSchemeColor(_themeType, _colorScheme.get(material::ColorScheme::Name(i)), _colorScheme.on(material::ColorScheme::Name(i)));
	}

	switch (_themeType) {
	case material::ColorScheme::LightTheme:
		_huePicker->setLabelColor(Color::Black);
		_chromaPicker->setLabelColor(Color::Black);
		_tonePicker->setLabelColor(Color::Black);
		_lightCheckbox->setLabelColor(Color::Black);
		_contentCheckbox->setLabelColor(Color::Black);
		break;
	case material::ColorScheme::DarkTheme:
		_huePicker->setLabelColor(Color::White);
		_chromaPicker->setLabelColor(Color::White);
		_tonePicker->setLabelColor(Color::White);
		_lightCheckbox->setLabelColor(Color::White);
		_contentCheckbox->setLabelColor(Color::White);
		break;
	}
}

}
