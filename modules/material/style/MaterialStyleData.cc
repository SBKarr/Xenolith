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

#include "MaterialStyleContainer.h"
#include "MaterialStyleData.h"

namespace stappler::xenolith::material {

StyleData StyleData::Background = StyleData{String(), ColorRole::Background, Elevation::Level0};

StyleData StyleData::progress(const StyleData &l, const StyleData &r, float p) {
	StyleData ret(r);
	ret.colorHCT = stappler::progress(l.colorHCT, r.colorHCT, p);
	ret.colorBackground = stappler::progress(l.colorBackground, r.colorBackground, p);
	ret.colorScheme = ret.colorHCT;
	ret.elevationValue = stappler::progress(l.elevationValue, r.elevationValue, p);

	ret.colorElevation = ret.colorBackground.asColor4F() * (1.0f - ret.elevationValue) + ret.colorScheme * ret.elevationValue;

	if (l.shapeFamily == r.shapeFamily) {
		ret.cornerRadius = stappler::progress(l.cornerRadius, r.cornerRadius, p);
	} else {
		auto scale = l.cornerRadius / (l.cornerRadius + r.cornerRadius);
		if (p < scale) {
			ret.shapeStyle = l.shapeStyle;
			ret.cornerRadius = stappler::progress(l.cornerRadius, 0.0f, p / scale);
		} else {
			ret.shapeStyle = r.shapeStyle;
			ret.cornerRadius = stappler::progress(0.0f, r.cornerRadius, p / (1.0f - scale));
		}
	}

	ret.shadowValue = stappler::progress(l.shadowValue, r.shadowValue, p);

	return ret;
}

bool StyleData::apply(const Size2 &contentSize, const StyleContainer *style) {
	bool dirty = false;

	// HCT resolve is expensive, compare only HCT values before it
	ColorHCT::Values targetColorHCT, targetColorBackground, targetColorOn;

	const ColorScheme *scheme = nullptr;

	if (schemeName.empty()) {
		scheme = &style->getPrimaryScheme();
	} else {
		if (auto s = style->getExtraScheme(schemeName)) {
			scheme = s;
		} else {
			scheme = &style->getPrimaryScheme();
		}
	}

	themeType = scheme->type;

	bool hasShadow = false;

	switch (nodeStyle) {
	case NodeStyle::Tonal:
		targetColorHCT = scheme->values(colorRule);
		targetColorBackground = scheme->values(ColorRole::Background);
		targetColorOn = scheme->values(getColorRoleOn(ColorRole::Background, scheme->type));
		break;
	case NodeStyle::TonalElevated:
		targetColorHCT = scheme->values(colorRule);
		targetColorBackground = scheme->values(ColorRole::Background);
		targetColorOn = scheme->values(getColorRoleOn(ColorRole::Background, scheme->type));
		hasShadow = true;
		break;
	case NodeStyle::Elevated:
		targetColorHCT = scheme->values(ColorRole::Background);
		targetColorBackground = scheme->values(ColorRole::Background);
		targetColorOn = scheme->values(getColorRoleOn(ColorRole::Background, scheme->type));
		hasShadow = true;
		break;
	case NodeStyle::Filled:
		targetColorHCT = scheme->values(colorRule);
		targetColorBackground = scheme->values(colorRule);
		targetColorOn = scheme->values(getColorRoleOn(colorRule, scheme->type));
		break;
	case NodeStyle::FilledElevated:
		targetColorHCT = scheme->values(colorRule);
		targetColorBackground = scheme->values(colorRule);
		targetColorOn = scheme->values(getColorRoleOn(colorRule, scheme->type));
		hasShadow = true;
		break;
	case NodeStyle::Outlined:
		targetColorHCT = scheme->values(ColorRole::Background);
		targetColorBackground = scheme->values(ColorRole::Background);
		targetColorOn = scheme->values(getColorRoleOn(ColorRole::Background, scheme->type));
		break;
	case NodeStyle::Text:
		targetColorHCT = scheme->values(ColorRole::Background); targetColorHCT.alpha = 0.0f;
		targetColorBackground = scheme->values(ColorRole::Background); targetColorBackground.alpha = 0.0f;
		targetColorOn = scheme->values(getColorRoleOn(ColorRole::Background, scheme->type));
		break;
	}

	if (targetColorHCT != colorHCT.data) {
		colorHCT = targetColorHCT;
		colorScheme = colorHCT.asColor4F();
		dirty = true;
	}
	if (targetColorBackground != colorBackground.data) {
		colorBackground = targetColorBackground;
		dirty = true;
	}

	float targetElevationValue = 0.0f;
	float targetShadowValue = 0.0f;

	switch (elevation) {
	case Elevation::Level0:
		targetElevationValue = 0.0f;
		targetShadowValue = hasShadow ? 0.0f : 0.0f;
		break;	// 0dp
	case Elevation::Level1:
		targetElevationValue = 0.05f;
		targetShadowValue = hasShadow ? 1.0f : 0.0f;
		break; // 1dp 5%
	case Elevation::Level2:
		targetElevationValue = 0.08f;
		targetShadowValue = hasShadow ? 3.0f : 0.0f;
		break; // 3dp 8%
	case Elevation::Level3:
		targetElevationValue = 0.11f;
		targetShadowValue = hasShadow ? 5.5f : 0.0f;
		break; // 6dp 11%
	case Elevation::Level4:
		targetElevationValue = 0.12f;
		targetShadowValue = hasShadow ? 7.0f : 0.0f;
		break; // 8dp 12%
	case Elevation::Level5:
		targetElevationValue = 0.14f;
		targetShadowValue = hasShadow ? 10.0f : 0.0f;
		break; // 12dp 14%
	}

	if (targetElevationValue != elevationValue) {
		elevationValue = targetElevationValue;
		dirty = true;
	}

	if (dirty) {
		colorElevation = colorBackground.asColor4F() * (1.0f - elevationValue) + colorScheme * elevationValue;
		colorElevation.a = 1.0f; // ensure opacity
	}

	if (targetColorOn != colorOn.data) {
		colorOn = targetColorOn;
		dirty = true;
	}

	if (targetShadowValue != shadowValue) {
		shadowValue = targetShadowValue;
		dirty = true;
	}

	float targetCornerRadius = 0.0f;

	switch (shapeStyle) {
	case ShapeStyle::None: targetCornerRadius = 0.0f; break; // 0dp
	case ShapeStyle::ExtraSmall: targetCornerRadius = 4.0f; break; // 4dp
	case ShapeStyle::Small: targetCornerRadius = 8.0f; break; // 8dp
	case ShapeStyle::Medium: targetCornerRadius = 12.0f; break; // 12dp
	case ShapeStyle::Large: targetCornerRadius = 16.0f; break; // 16dp
	case ShapeStyle::ExtraLarge: targetCornerRadius = 28.0f; break; // 28dp
	case ShapeStyle::Full: targetCornerRadius = std::min(contentSize.width, contentSize.height) / 2.0f; break; // .
	}

	if (targetCornerRadius != cornerRadius) {
		cornerRadius = targetCornerRadius;
		dirty = true;
	}

	return dirty;
}

}
