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

#ifndef MODULES_MATERIAL_BASE_MATERIALCOLORSCHEME_H_
#define MODULES_MATERIAL_BASE_MATERIALCOLORSCHEME_H_

#include "MaterialColorHCT.h"

namespace stappler::xenolith::material {

enum class ThemeType {
	LightTheme,
	DarkTheme,
};

enum class ColorRole {
	Primary,
	OnPrimary,
	PrimaryContainer,
	OnPrimaryContainer,
	Secondary,
	OnSecondary,
	SecondaryContainer,
	OnSecondaryContainer,
	Tertiary,
	OnTertiary,
	TertiaryContainer,
	OnTertiaryContainer,
	Error,
	OnError,
	ErrorContainer,
	OnErrorContainer,
	Background,
	OnBackground,
	Surface,
	OnSurface,
	SurfaceVariant,
	OnSurfaceVariant,
	Outline,
	OutlineVariant,
	Shadow,
	Scrim,
	InverseSurface,
	InverseOnSurface,
	InversePrimary,
	Max
};

struct TonalPalette {
	TonalPalette() = default;

	explicit TonalPalette(const Color4F &color)
	: TonalPalette(Cam16::create(color)) { }

	explicit TonalPalette(const Cam16 &cam)
	: hue(cam.hue), chroma(cam.chroma) { }

	TonalPalette(Cam16Float hue, Cam16Float chroma)
	: hue(hue), chroma(chroma) { }

	Color4F get(Cam16Float tone, float alpha = 1.0f) const {
		return ColorHCT::solveColor4F(hue, chroma, tone, alpha);
	}

	ColorHCT hct(Cam16Float tone, float alpha = 1.0f) const {
		return ColorHCT(hue, chroma, tone, alpha);
	}

	ColorHCT::Values values(Cam16Float tone, float alpha = 1.0f) const {
		return ColorHCT::Values{hue, chroma, tone, alpha};
	}

	Cam16Float hue = Cam16Float(0.0);
	Cam16Float chroma = Cam16Float(0.5);
};

/**
 * An intermediate concept between the key color for a UI theme, and a full
 * color scheme. 5 tonal palettes are generated, all except one use the same
 * hue as the key color, and all vary in chroma.
 */
struct CorePalette {
	CorePalette() = default;
	CorePalette(const Color4F &, bool isContentColor);
	CorePalette(const Cam16 &, bool isContentColor);
	CorePalette(Cam16Float hue, Cam16Float chroma, bool isContentColor);

	TonalPalette primary;
	TonalPalette secondary;
	TonalPalette tertiary;
	TonalPalette neutral;
	TonalPalette neutralVariant;
	TonalPalette error;
};

struct ColorScheme {
	static ColorRole getColorRoleOn(ColorRole, ThemeType);

	ColorScheme() = default;
	ColorScheme(ThemeType, const CorePalette &);
	ColorScheme(ThemeType, const Color4F &, bool isContent);
	ColorScheme(ThemeType, const ColorHCT &, bool isContent);

	void set(ThemeType, const CorePalette &);
	void set(ThemeType, const Color4F &, bool isContent);
	void set(ThemeType, const ColorHCT &, bool isContent);

	Color4F get(ColorRole name) const {
		return colors[toInt(name)];
	}

	Color4F on(ColorRole name) const {
		return colors[toInt(getColorRoleOn(name, type))];
	}

	ColorHCT hct(ColorRole name, float alpha = 1.0f) const;

	// faster then complete color
	ColorHCT::Values values(ColorRole name, float alpha = 1.0f) const;

	ThemeType type = ThemeType::LightTheme;
	std::array<Color4F, toInt(ColorRole::Max)> colors;
	CorePalette palette;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALCOLORSCHEME_H_ */
