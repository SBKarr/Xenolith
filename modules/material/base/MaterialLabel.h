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

#ifndef MODULES_MATERIAL_BASE_MATERIALLABEL_H_
#define MODULES_MATERIAL_BASE_MATERIALLABEL_H_

#include "MaterialColorScheme.h"
#include "XLLabel.h"

namespace stappler::xenolith::material {

enum class TypescaleRole {
	DisplayLarge, // 57 400
	DisplayMedium, // 45 400
	DisplaySmall, // 36 400
	HeadlineLarge, // 32 400
	HeadlineMedium, // 28 400
	HeadlineSmall, // 24 400
	TitleLarge, // 22 400
	TitleMedium, // 16 500
	TitleSmall, // 14 500
	LabelLarge, // 14 500
	LabelMedium, // 12 500
	LabelSmall, // 11 500
	BodyLarge, // 16 400 0.5
	BodyMedium, // 14 400 0.25
	BodySmall, // 12 400 0.4
	Unknown,
};

class TypescaleLabel : public Label {
public:
	virtual ~TypescaleLabel() { }

	virtual bool init(TypescaleRole);
	virtual bool init(TypescaleRole, StringView);
	virtual bool init(TypescaleRole, StringView, float w, Alignment = Alignment::Left);

	virtual TypescaleRole getRole() const { return _role; }
	virtual void setRole(TypescaleRole);

protected:
	virtual bool visitDraw(RenderFrameInfo &, NodeFlags parentFlags) override;
	virtual void specializeStyle(DescriptionStyle &style, float density) const override;

	using Label::init;

	TypescaleRole _role = TypescaleRole::Unknown;
	ThemeType _themeType = ThemeType::LightTheme;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALLABEL_H_ */
