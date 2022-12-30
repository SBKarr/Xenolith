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

#ifndef MODULES_MATERIAL_MATERIAL_H_
#define MODULES_MATERIAL_MATERIAL_H_

#include "XLDefine.h"

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
	OnSecondary ,
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

enum class Elevation {
	Level0,	// 0dp
	Level1, // 1dp 5%
	Level2, // 3dp 8%
	Level3, // 6dp 11%
	Level4, // 8dp 12%
	Level5, // 12dp 14%
};

enum class ShapeFamily {
	RoundedCorners,
	CutCorners,
};

enum class ShapeStyle {
	None, // 0dp
	ExtraSmall, // 4dp
	Small, // 8dp
	Medium, // 12dp
	Large, // 16dp
	ExtraLarge, // 28dp
	Full // .
};

ColorRole getColorRoleOn(ColorRole, ThemeType);

}

#endif /* MODULES_MATERIAL_MATERIAL_H_ */
