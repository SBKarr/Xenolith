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

#ifndef MODULES_MATERIAL_BASE_MATERIALSTYLECONTAINER_H_
#define MODULES_MATERIAL_BASE_MATERIALSTYLECONTAINER_H_

#include "MaterialSurfaceStyle.h"
#include "XLComponent.h"
#include "XLEventHeader.h"

namespace stappler::xenolith::material {

class StyleContainer : public Component {
public:
	static EventHeader onColorSchemeUpdate;
	static uint64_t ComponentFrameTag;
	static constexpr uint32_t PrimarySchemeTag = SurfaceStyle::PrimarySchemeTag;

	virtual ~StyleContainer() { }

	virtual bool init() override;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	void setPrimaryScheme(ThemeType, const CorePalette &);
	void setPrimaryScheme(ThemeType, const Color4F &, bool isContent);
	void setPrimaryScheme(ThemeType, const ColorHCT &, bool isContent);
	const ColorScheme &getPrimaryScheme() const;

	const ColorScheme *setScheme(uint32_t tag, ThemeType, const CorePalette &);
	const ColorScheme *setScheme(uint32_t tag, ThemeType, const Color4F &, bool isContent);
	const ColorScheme *setScheme(uint32_t tag, ThemeType, const ColorHCT &, bool isContent);
	const ColorScheme *getScheme(uint32_t) const;

	const Scene *getScene() const { return _scene; }

protected:
	Scene *_scene = nullptr;
	Map<uint32_t, ColorScheme> _schemes;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALSTYLECONTAINER_H_ */
