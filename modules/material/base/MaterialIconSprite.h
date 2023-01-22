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

#ifndef MODULES_MATERIAL_BASE_MATERIALICONSPRITE_H_
#define MODULES_MATERIAL_BASE_MATERIALICONSPRITE_H_

#include "XLIconNames.h"
#include "XLVectorSprite.h"
#include "MaterialSurfaceInterior.h"

namespace stappler::xenolith::material {

class IconSprite : public VectorSprite {
public:
	virtual ~IconSprite() { }

	virtual bool init(IconName);

	virtual IconName getIconName() const { return _iconName; }
	virtual void setIconName(IconName);

	virtual bool visitDraw(RenderFrameInfo &, NodeFlags parentFlags) override;

protected:
	using VectorSprite::init;

	virtual void updateIcon();

	IconName _iconName = IconName::None;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALICONSPRITE_H_ */
