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

#ifndef MODULES_GUI_XLGUILAYERROUNDED_H_
#define MODULES_GUI_XLGUILAYERROUNDED_H_

#include "XLVectorSprite.h"

namespace stappler::xenolith {

class LayerRounded : public VectorSprite {
public:
	virtual ~LayerRounded() { }

	virtual bool init(const Color4F &, float borderRadius);

	virtual void onContentSizeDirty() override;

	virtual void setBorderRadius(float);
	virtual float getBorderRadius() const { return _borderRadius; }

	virtual void setPathColor(const Color4B &, bool withOpaity);
	virtual const Color4B &getPathColor() const;

protected:
	using VectorSprite::init;

	Color4B _pathColor = Color::White;
	float _borderRadius = 0.0f;
	float _realBorderRadius = 0.0f;
};

}

#endif /* MODULES_GUI_XLGUILAYERROUNDED_H_ */
