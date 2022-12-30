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

#ifndef MODULES_MATERIAL_MATERIALSTYLECONTAINER_H_
#define MODULES_MATERIAL_MATERIALSTYLECONTAINER_H_

#include "MaterialColorScheme.h"
#include "XLComponent.h"
#include "XLEventHeader.h"

namespace stappler::xenolith::material {

class StyleContainer : public Component {
public:
	static EventHeader onAttached; // sent when container enabled or disabled
	static EventHeader onPrimaryColorSchemeUpdate;
	static EventHeader onExtraColorSchemeUpdate;

	virtual ~StyleContainer() { }

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	void setPrimaryScheme(ThemeType, const CorePalette &);
	void setPrimaryScheme(ThemeType, const Color4F &, bool isContent);
	void setPrimaryScheme(ThemeType, const ColorHCT &, bool isContent);
	const ColorScheme &getPrimaryScheme() const;

	const ColorScheme *setExtraScheme(StringView, ThemeType, const CorePalette &);
	const ColorScheme *setExtraScheme(StringView, ThemeType, const Color4F &, bool isContent);
	const ColorScheme *setExtraScheme(StringView, ThemeType, const ColorHCT &, bool isContent);
	const ColorScheme *getExtraScheme(StringView) const;

protected:
	ColorScheme _primaryScheme = ColorScheme(ThemeType::LightTheme, ColorHCT(292, 100, 50, 1.0f), false);
	Map<String, ColorScheme> _extraSchemes;
};

}

#endif /* MODULES_MATERIAL_MATERIALSTYLECONTAINER_H_ */
