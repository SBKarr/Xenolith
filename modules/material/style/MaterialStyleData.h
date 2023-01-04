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

#ifndef MODULES_MATERIAL_STYLE_MATERIALSTYLEDATA_H_
#define MODULES_MATERIAL_STYLE_MATERIALSTYLEDATA_H_

#include "MaterialColorScheme.h"

namespace stappler::xenolith::material {

class StyleContainer;

struct StyleData {
	static StyleData progress(const StyleData &, const StyleData &, float p);
	static StyleData Background;

	bool apply(const Size2 &contentSize, const StyleContainer *);

	bool operator==(const StyleData &) const = default;
	bool operator!=(const StyleData &) const = default;

	String schemeName;
	ColorRole colorRule = ColorRole::Primary;
	Elevation elevation = Elevation::Level0;
	ShapeFamily shapeFamily = ShapeFamily::RoundedCorners;
	ShapeStyle shapeStyle = ShapeStyle::None;
	NodeStyle nodeStyle = NodeStyle::Tonal;

	ThemeType themeType = ThemeType::LightTheme;
	Color4F colorScheme;
	Color4F colorElevation;
	ColorHCT colorHCT;
	ColorHCT colorBackground;
	ColorHCT colorOn;
	float cornerRadius = 0.0f;
	float elevationValue = 0.0f;
	float shadowValue = 0.0f;
};

}

#endif /* MODULES_MATERIAL_STYLE_MATERIALSTYLEDATA_H_ */
