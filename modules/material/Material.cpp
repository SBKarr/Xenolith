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

#include "XLDefine.h"
#include "Material.h"
#include "MaterialColorScheme.cc"
#include "MaterialStyleContainer.cc"
#include "MaterialNode.cc"

namespace stappler::xenolith::material {

ColorRole getColorRoleOn(ColorRole role, ThemeType type) {
	switch (role) {
	case ColorRole::Primary: return ColorRole::OnPrimary; break;
	case ColorRole::OnPrimary: return ColorRole::Primary; break;
	case ColorRole::PrimaryContainer: return ColorRole::OnPrimaryContainer; break;
	case ColorRole::OnPrimaryContainer: return ColorRole::PrimaryContainer; break;
	case ColorRole::Secondary: return ColorRole::OnSecondary; break;
	case ColorRole::OnSecondary: return ColorRole::Secondary; break;
	case ColorRole::SecondaryContainer: return ColorRole::OnSecondaryContainer; break;
	case ColorRole::OnSecondaryContainer: return ColorRole::SecondaryContainer; break;
	case ColorRole::Tertiary: return ColorRole::OnTertiary; break;
	case ColorRole::OnTertiary: return ColorRole::Tertiary; break;
	case ColorRole::TertiaryContainer: return ColorRole::OnTertiaryContainer; break;
	case ColorRole::OnTertiaryContainer: return ColorRole::TertiaryContainer; break;
	case ColorRole::Error: return ColorRole::OnError; break;
	case ColorRole::OnError: return ColorRole::Error; break;
	case ColorRole::ErrorContainer: return ColorRole::OnErrorContainer; break;
	case ColorRole::OnErrorContainer: return ColorRole::ErrorContainer; break;
	case ColorRole::Background: return ColorRole::OnBackground; break;
	case ColorRole::OnBackground: return ColorRole::Background; break;
	case ColorRole::Surface: return ColorRole::OnSurface; break;
	case ColorRole::OnSurface: return ColorRole::Surface; break;
	case ColorRole::SurfaceVariant: return ColorRole::OnSurfaceVariant; break;
	case ColorRole::OnSurfaceVariant: return ColorRole::SurfaceVariant; break;
	case ColorRole::Outline: return ColorRole::OnBackground; break;
	case ColorRole::OutlineVariant: return ColorRole::OnBackground; break;
	case ColorRole::Shadow: return (type == ThemeType::LightTheme) ? ColorRole::Background : ColorRole::OnBackground; break;
	case ColorRole::Scrim: return (type == ThemeType::LightTheme) ? ColorRole::Background : ColorRole::OnBackground; break;
	case ColorRole::InverseSurface: return ColorRole::InverseOnSurface; break;
	case ColorRole::InverseOnSurface: return ColorRole::InverseSurface; break;
	case ColorRole::InversePrimary: return ColorRole::OnBackground; break;
	case ColorRole::Max: break;
	}
	return role;
}

}
