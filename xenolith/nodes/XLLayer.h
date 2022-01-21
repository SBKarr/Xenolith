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

#ifndef XENOLITH_NODES_XLLAYER_H_
#define XENOLITH_NODES_XLLAYER_H_

#include "XLSprite.h"

namespace stappler::xenolith {

struct SimpleGradient {
	using Color = Color4B;
	using ColorRef = const Color &;

	static const Vec2 Vertical;
	static const Vec2 Horizontal;

	static SimpleGradient progress(const SimpleGradient &a, const SimpleGradient &b, float p);

	SimpleGradient();
	SimpleGradient(ColorRef start, ColorRef end, const Vec2 & = Vertical);
	SimpleGradient(ColorRef bl, ColorRef br, ColorRef tl, ColorRef tr);

	Color4B colors[4]; // bl - br - tl - tr
};

/**
 *  Layer is a simple layout sprite, colored with solid color or simple linear gradient
 */
class Layer : public Sprite {
public:
	virtual ~Layer() { }

	virtual bool init(const Color4F & = Color4F::WHITE);
	virtual bool init(const SimpleGradient &);

	virtual void onContentSizeDirty() override;

	virtual void setGradient(const SimpleGradient &);
	virtual const SimpleGradient &getGradient() const;

protected:
	virtual void updateVertexes() override;
	virtual void updateVertexesColor() override;

	SimpleGradient _gradient;
};

}

namespace stappler {

template <> inline
xenolith::SimpleGradient progress<xenolith::SimpleGradient>(const xenolith::SimpleGradient &a, const xenolith::SimpleGradient &b, float p) {
	return xenolith::SimpleGradient::progress(a, b, p);
}

}

#endif /* XENOLITH_NODES_XLLAYER_H_ */
