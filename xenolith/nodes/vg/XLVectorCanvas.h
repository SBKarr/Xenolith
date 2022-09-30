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

#ifndef XENOLITH_NODES_VG_XLVECTORCANVAS_H_
#define XENOLITH_NODES_VG_XLVECTORCANVAS_H_

#include "XLDefine.h"
#include "SPVectorImage.h"
#include "XLGl.h"

namespace stappler::xenolith {

using VectorPath = stappler::vg::VectorPath;
using VectorImageData = stappler::vg::VectorImageData;
using VectorImage = stappler::vg::VectorImage;
using VectorPathRef = stappler::vg::VectorPathRef;

struct VectorCanvasResult : public Ref {
	Vector<Pair<Mat4, Rc<gl::VertexData>>> data;
	Vector<Pair<Mat4, Rc<gl::VertexData>>> mut;
	Color4F targetColor;
	Size2 targetSize;
	Mat4 targetTransform;

	void updateColor(const Color4F &);
};

class VectorCanvas : public Ref {
public:
	static Rc<VectorCanvas> getInstance();

	virtual ~VectorCanvas();

	bool init(float quality = 0.75f, Color4F color = Color4F::WHITE);

	void setColor(Color4F);
	Color4F getColor() const;

	void setQuality(float);
	float getQuality() const;

	Rc<VectorCanvasResult> draw(Rc<VectorImageData> &&, Size2 targetSize);

protected:
	struct Data;

	Data *_data;
};

}

#endif /* XENOLITH_NODES_VG_XLVECTORCANVAS_H_ */
