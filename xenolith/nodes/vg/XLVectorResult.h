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

#ifndef XENOLITH_NODES_VG_XLVECTORRESULT_H_
#define XENOLITH_NODES_VG_XLVECTORRESULT_H_

#include "XLGl.h"
#include "SPVectorImage.h"
#include <future>

namespace stappler::xenolith {

using VectorImageData = vg::VectorImageData;
using VectorImage = vg::VectorImage;
using VectorPathRef = vg::VectorPathRef;

struct VectorCanvasResult : public Ref {
	Vector<gl::TransformedVertexData> data;
	Vector<gl::TransformedVertexData> mut;
	Color4F targetColor;
	Size2 targetSize;
	Mat4 targetTransform;

	void updateColor(const Color4F &);
};

class VectorCanvasDeferredResult : public gl::DeferredVertexResult {
public:
	virtual ~VectorCanvasDeferredResult();

	bool init(std::future<Rc<VectorCanvasResult>> &&);

	virtual const Vector<gl::TransformedVertexData> &getData() override;

	virtual void handleReady(Rc<VectorCanvasResult> &&);

	void updateColor(const Color4F &);

	Rc<VectorCanvasResult> getResult() const;

protected:
	mutable Mutex _mutex;
	Rc<VectorCanvasResult> _result;
	std::future<Rc<VectorCanvasResult>> *_future = nullptr;
};

}

#endif /* XENOLITH_NODES_VG_XLVECTORRESULT_H_ */
