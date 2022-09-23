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

#ifndef XENOLITH_NODES_XLVECTORSPRITE_H_
#define XENOLITH_NODES_XLVECTORSPRITE_H_

#include "XLSprite.h"
#include "XLVectorCanvas.h"

namespace stappler::xenolith {

class VectorSprite : public Sprite {
public:
	constexpr static float QualityWorst = 0.1f;
	constexpr static float QualityLow = 0.25f;
	constexpr static float QualityNormal = 0.75f;
	constexpr static float QualityHigh = 1.25f;
	constexpr static float QualityPerfect = 1.75f;

	virtual ~VectorSprite() { }

	VectorSprite();

	virtual bool init(Rc<VectorImage> &&);
	virtual bool init(Size2, StringView);
	virtual bool init(Size2, VectorPath &&);
	virtual bool init(Size2);
	virtual bool init(StringView);
	virtual bool init(BytesView);
	virtual bool init(FilePath);

    virtual Rc<VectorPathRef> addPath(StringView id = StringView(), StringView cache = StringView(), Mat4 = Mat4::IDENTITY);
    virtual Rc<VectorPathRef> addPath(const VectorPath & path, StringView id = StringView(), StringView cache = StringView(), Mat4 = Mat4::IDENTITY);
    virtual Rc<VectorPathRef> addPath(VectorPath && path, StringView id = StringView(), StringView cache = StringView(), Mat4 = Mat4::IDENTITY);

    virtual Rc<VectorPathRef> getPath(StringView);

    virtual void removePath(const Rc<VectorPathRef> &);
    virtual void removePath(StringView);

    virtual void clear();

    virtual void setImage(Rc<VectorImage> &&);
    virtual const Rc<VectorImage> &getImage() const;

    virtual void setQuality(float);
    virtual float getQuality() const { return _quality; }

	virtual void onTransformDirty(const Mat4 &) override;

	virtual bool visitDraw(RenderFrameInfo &, NodeFlags parentFlags) override;

	virtual uint32_t getTrianglesCount() const;
	virtual uint32_t getVertexesCount() const;

protected:
	virtual void pushCommands(RenderFrameInfo &, NodeFlags flags) override;

	virtual void initVertexes() override;
	virtual void updateVertexes() override;
	virtual void updateVertexesColor() override;

	bool _async = false;
	uint64_t _asyncJobId = 0;
	Size2 _targetSize;
	Mat4 _targetTransform;
	Rc<VectorImage> _image;
	float _quality = QualityNormal;
	Rc<VectorCanvasResult> _result;
};

}

#endif /* XENOLITH_NODES_XLVECTORSPRITE_H_ */
