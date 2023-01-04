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

#include "MaterialLabel.h"
#include "MaterialNode.h"

namespace stappler::xenolith::material {

bool MaterialLabel::init(TypescaleRole role) {
	if (!Label::init()) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

bool MaterialLabel::init(TypescaleRole role, StringView str) {
	if (!Label::init(str)) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

bool MaterialLabel::init(TypescaleRole role, StringView str, float w, Alignment a) {
	if (!Label::init(str, w, a)) {
		return false;
	}

	setFontFamily("sans");
	setRole(role);
	return true;
}

void MaterialLabel::setRole(TypescaleRole role) {
	_role = role;
	switch (_role) {
	case TypescaleRole::DisplayLarge: setFontSize(57); setFontWeight(FontWeight(400)); break; // 57 400
	case TypescaleRole::DisplayMedium: setFontSize(45); setFontWeight(FontWeight(400)); break; // 45 400
	case TypescaleRole::DisplaySmall: setFontSize(36); setFontWeight(FontWeight(400)); break; // 36 400
	case TypescaleRole::HeadlineLarge: setFontSize(32); setFontWeight(FontWeight(400)); break; // 32 400
	case TypescaleRole::HeadlineMedium: setFontSize(28); setFontWeight(FontWeight(400)); break; // 28 400
	case TypescaleRole::HeadlineSmall: setFontSize(24); setFontWeight(FontWeight(400)); break; // 24 400
	case TypescaleRole::TitleLarge: setFontSize(22); setFontWeight(FontWeight(400)); break; // 22 400
	case TypescaleRole::TitleMedium: setFontSize(16); setFontWeight(FontWeight(500)); break; // 16 500
	case TypescaleRole::TitleSmall: setFontSize(14); setFontWeight(FontWeight(500)); break; // 14 500
	case TypescaleRole::LabelLarge: setFontSize(14); setFontWeight(FontWeight(500)); break; // 14 500
	case TypescaleRole::LabelMedium: setFontSize(12); setFontWeight(FontWeight(500)); break; // 12 500
	case TypescaleRole::LabelSmall: setFontSize(11); setFontWeight(FontWeight(500)); break; // 11 500
	case TypescaleRole::BodyLarge: setFontSize(16); setFontWeight(FontWeight(400)); break; // 16 400 0.5
	case TypescaleRole::BodyMedium: setFontSize(14); setFontWeight(FontWeight(400)); break; // 14 400 0.25
	case TypescaleRole::BodySmall: setFontSize(12); setFontWeight(FontWeight(400)); break; // 12 400 0.4
	case TypescaleRole::Unknown: break;
	}
}

bool MaterialLabel::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = frame.getComponent<MaterialNodeInterior>(MaterialNodeInterior::ComponentFrameTag);
	if (style) {
		auto color = style->getStyle().colorOn.asColor4F();
		setColor(color);

		if (_themeType != style->getStyle().themeType) {
			_themeType = style->getStyle().themeType;
			_labelDirty = true;
		}
	}

	return Label::visitDraw(frame, parentFlags);
}

void MaterialLabel::specializeStyle(DescriptionStyle &style, float density) const {
	struct PersistentStyle {
		FontSize size;
		FontWeight weight;

		constexpr PersistentStyle(TypescaleRole role) {
			switch (role) {
			case TypescaleRole::DisplayLarge: size = FontSize(57); weight = FontWeight(400); break; // 57 400
			case TypescaleRole::DisplayMedium: size = FontSize(45); weight = FontWeight(400); break; // 45 400
			case TypescaleRole::DisplaySmall: size = FontSize(36); weight = FontWeight(400); break; // 36 400
			case TypescaleRole::HeadlineLarge: size = FontSize(32); weight = FontWeight(400); break; // 32 400
			case TypescaleRole::HeadlineMedium: size = FontSize(28); weight = FontWeight(400); break; // 28 400
			case TypescaleRole::HeadlineSmall: size = FontSize(24); weight = FontWeight(400); break; // 24 400
			case TypescaleRole::TitleLarge: size = FontSize(22); weight = FontWeight(400); break; // 22 400
			case TypescaleRole::TitleMedium: size = FontSize(16); weight = FontWeight(500); break; // 16 500
			case TypescaleRole::TitleSmall: size = FontSize(14); weight = FontWeight(500); break; // 14 500
			case TypescaleRole::LabelLarge: size = FontSize(14); weight = FontWeight(500); break; // 14 500
			case TypescaleRole::LabelMedium: size = FontSize(12); weight = FontWeight(500); break; // 12 500
			case TypescaleRole::LabelSmall: size = FontSize(11); weight = FontWeight(500); break; // 11 500
			case TypescaleRole::BodyLarge: size = FontSize(16); weight = FontWeight(400); break; // 16 400 0.5
			case TypescaleRole::BodyMedium: size = FontSize(14); weight = FontWeight(400); break; // 14 400 0.25
			case TypescaleRole::BodySmall: size = FontSize(12); weight = FontWeight(400); break; // 12 400 0.4
			case TypescaleRole::Unknown: break;
			}
		}

		bool operator==(const FontParameters &f) const { return f.fontSize == size && f.fontWeight == weight; }
	};

	if (_themeType == ThemeType::DarkTheme) {
		style.font.fontGrade = FontGrade(style.font.fontGrade.get() - 50);
	}

	Label::specializeStyle(style, density);
	if (!_persistentLayout) {
		if ((style.font.fontGrade == FontGrade::Normal || style.font.fontGrade == FontGrade::Normal)
				&& style.font.fontStretch.get() % 100 == 0
				&& (style.font.fontStyle == FontStyle::Italic || style.font.fontStyle == FontStyle::Oblique)) {

			static constexpr PersistentStyle variants[] = {
				PersistentStyle(TypescaleRole::DisplayLarge), // 57 400
				PersistentStyle(TypescaleRole::DisplayMedium), // 45 400
				PersistentStyle(TypescaleRole::DisplaySmall), // 36 400
				PersistentStyle(TypescaleRole::HeadlineLarge), // 32 400
				PersistentStyle(TypescaleRole::HeadlineMedium), // 28 400
				PersistentStyle(TypescaleRole::HeadlineSmall), // 24 400
				PersistentStyle(TypescaleRole::TitleLarge), // 22 400
				PersistentStyle(TypescaleRole::TitleMedium), // 16 500
				PersistentStyle(TypescaleRole::TitleSmall), // 14 500
				PersistentStyle(TypescaleRole::LabelLarge), // 14 500
				PersistentStyle(TypescaleRole::LabelMedium), // 12 500
				PersistentStyle(TypescaleRole::LabelSmall), // 11 500
				PersistentStyle(TypescaleRole::BodyLarge), // 16 400 0.5
				PersistentStyle(TypescaleRole::BodyMedium), // 14 400 0.25
				PersistentStyle(TypescaleRole::BodySmall), // 12 400 0.4
			};

			for (auto &it : variants) {
				if (it == style.font) {
					style.font.persistent = true;
				}
			}
		}
	}
}

}
