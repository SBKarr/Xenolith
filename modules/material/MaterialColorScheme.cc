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

#include "MaterialColorScheme.h"

namespace stappler::xenolith::material {

CorePalette::CorePalette(const Color4F &color, bool isContentColor)
: CorePalette(Cam16::create(color), isContentColor) { }

CorePalette::CorePalette(const Cam16 &cam, bool isContentColor)
: CorePalette(cam.hue, cam.chroma, isContentColor) { }

CorePalette::CorePalette(Cam16Float hue, Cam16Float chroma, bool is_content)
: primary(hue, is_content ? chroma : std::fmax(chroma, Cam16Float(48)))
, secondary(hue, is_content ? chroma / 3 : 16)
, tertiary(hue + 60, is_content ? chroma / 2 : 24)
, neutral(hue, is_content ? std::fmin(chroma / Cam16Float(12), Cam16Float(4)) : 4)
, neutralVariant(hue, is_content ? std::fmin(chroma / Cam16Float(6), Cam16Float(8)) : 8)
, error(25, 84) { }

ColorScheme::ColorScheme(Type type, const CorePalette &palette) : type(type) {
	switch (type) {
	case LightTheme:
		colors[Primary] = Color4B(palette.primary.get(40));
		colors[OnPrimary] = Color4B(palette.primary.get(100));
		colors[PrimaryContainer] = Color4B(palette.primary.get(90));
		colors[OnPrimaryContainer] = Color4B(palette.primary.get(10));
		colors[Secondary] = Color4B(palette.secondary.get(40));
		colors[OnSecondary] = Color4B(palette.secondary.get(100));
		colors[SecondaryContainer] = Color4B(palette.secondary.get(90));
		colors[OnSecondaryContainer] = Color4B(palette.secondary.get(10));
		colors[Tertiary] = Color4B(palette.tertiary.get(40));
		colors[OnTertiary] = Color4B(palette.tertiary.get(100));
		colors[TertiaryContainer] = Color4B(palette.tertiary.get(90));
		colors[OnTertiaryContainer] = Color4B(palette.tertiary.get(10));
		colors[Error] = Color4B(palette.error.get(40));
		colors[OnError] = Color4B(palette.error.get(100));
		colors[ErrorContainer] = Color4B(palette.error.get(90));
		colors[OnErrorContainer] = Color4B(palette.error.get(10));
		colors[Background] = Color4B(palette.neutral.get(99));
		colors[OnBackground] = Color4B(palette.neutral.get(10));
		colors[Surface] = Color4B(palette.neutral.get(99));
		colors[OnSurface] = Color4B(palette.neutral.get(10));
		colors[SurfaceVariant] = Color4B(palette.neutralVariant.get(90));
		colors[OnSurfaceVariant] = Color4B(palette.neutralVariant.get(30));
		colors[Outline] = Color4B(palette.neutralVariant.get(50));
		colors[OutlineVariant] = Color4B(palette.neutralVariant.get(80));
		colors[Shadow] = Color4B(palette.neutral.get(0));
		colors[Scrim] = Color4B(palette.neutral.get(0));
		colors[InverseSurface] = Color4B(palette.neutral.get(20));
		colors[InverseOnSurface] = Color4B(palette.neutral.get(95));
		colors[InversePrimary] = Color4B(palette.primary.get(80));
		break;
	case DarkTheme:
		colors[Primary] = Color4B(palette.primary.get(80));
		colors[OnPrimary] = Color4B(palette.primary.get(20));
		colors[PrimaryContainer] = Color4B(palette.primary.get(30));
		colors[OnPrimaryContainer] = Color4B(palette.primary.get(90));
		colors[Secondary] = Color4B(palette.secondary.get(80));
		colors[OnSecondary] = Color4B(palette.secondary.get(20));
		colors[SecondaryContainer] = Color4B(palette.secondary.get(30));
		colors[OnSecondaryContainer] = Color4B(palette.secondary.get(90));
		colors[Tertiary] = Color4B(palette.tertiary.get(80));
		colors[OnTertiary] = Color4B(palette.tertiary.get(20));
		colors[TertiaryContainer] = Color4B(palette.tertiary.get(30));
		colors[OnTertiaryContainer] = Color4B(palette.tertiary.get(90));
		colors[Error] = Color4B(palette.error.get(80));
		colors[OnError] = Color4B(palette.error.get(20));
		colors[ErrorContainer] = Color4B(palette.error.get(30));
		colors[OnErrorContainer] = Color4B(palette.error.get(80));
		colors[Background] = Color4B(palette.neutral.get(10));
		colors[OnBackground] = Color4B(palette.neutral.get(90));
		colors[Surface] = Color4B(palette.neutral.get(10));
		colors[OnSurface] = Color4B(palette.neutral.get(90));
		colors[SurfaceVariant] = Color4B(palette.neutralVariant.get(30));
		colors[OnSurfaceVariant] = Color4B(palette.neutralVariant.get(80));
		colors[Outline] = Color4B(palette.neutralVariant.get(60));
		colors[OutlineVariant] = Color4B(palette.neutralVariant.get(30));
		colors[Shadow] = Color4B(palette.neutral.get(0));
		colors[Scrim] = Color4B(palette.neutral.get(0));
		colors[InverseSurface] = Color4B(palette.neutral.get(90));
		colors[InverseOnSurface] = Color4B(palette.neutral.get(20));
		colors[InversePrimary] = Color4B(palette.primary.get(40));
		break;
	}
}

ColorScheme::ColorScheme(Type type, const Color4F &color, bool isContent)
: ColorScheme(type, CorePalette(color, isContent)) { }

ColorScheme::ColorScheme(Type type, const ColorHCT &color, bool isContent)
: ColorScheme(type, CorePalette(color.data.hue, color.data.chroma, isContent)) { }

Color4B ColorScheme::on(Name name) const {
	switch (name) {
	case Primary: return get(OnPrimary); break;
	case OnPrimary: return get(Primary); break;
	case PrimaryContainer: return get(OnPrimaryContainer); break;
	case OnPrimaryContainer: return get(PrimaryContainer); break;
	case Secondary: return get(OnSecondary); break;
	case OnSecondary: return get(Secondary); break;
	case SecondaryContainer: return get(OnSecondaryContainer); break;
	case OnSecondaryContainer: return get(SecondaryContainer); break;
	case Tertiary: return get(OnTertiary); break;
	case OnTertiary: return get(Tertiary); break;
	case TertiaryContainer: return get(OnTertiaryContainer); break;
	case OnTertiaryContainer: return get(TertiaryContainer); break;
	case Error: return get(OnError); break;
	case OnError: return get(Error); break;
	case ErrorContainer: return get(OnErrorContainer); break;
	case OnErrorContainer: return get(ErrorContainer); break;
	case Background: return get(OnBackground); break;
	case OnBackground: return get(Background); break;
	case Surface: return get(OnSurface); break;
	case OnSurface: return get(Surface); break;
	case SurfaceVariant: return get(OnSurfaceVariant); break;
	case OnSurfaceVariant: return get(SurfaceVariant); break;
	case Outline: return get(OnBackground); break;
	case OutlineVariant: return get(OnBackground); break;
	case Shadow: return get((type == LightTheme) ? Background : OnBackground); break;
	case Scrim: return get((type == LightTheme) ? Background : OnBackground); break;
	case InverseSurface: return get(InverseOnSurface); break;
	case InverseOnSurface: return get(InverseSurface); break;
	case InversePrimary: return get(OnBackground); break;
	case Max: break;
	}
	return Color4B::BLACK;
}

}
