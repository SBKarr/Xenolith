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

#ifndef XENOLITH_NODES_VG_XLVECTORIMAGE_H_
#define XENOLITH_NODES_VG_XLVECTORIMAGE_H_

#include "XLDefine.h"
#include "SLPath.h"

namespace stappler::xenolith {

class VectorImage;
class VectorImageData;

struct VectorCanvasResult : public Ref {
	Vector<Pair<Rc<gl::VertexData>, Mat4>> data;
	Color4F targetColor;
	Rect boxRect;

	void updateColor(const Color4F &);
};

class VectorPath : public Ref {
public:
	using Path = layout::Path;
	using DrawStyle = layout::DrawStyle;

	virtual ~VectorPath() { }

	bool init(VectorImage *, const String &, const Rc<Path> &);
	bool init(VectorImage *, const String &, Rc<Path> &&);

	size_t count() const;

	VectorPath & moveTo(float x, float y);
	VectorPath & moveTo(const Vec2 &point) {
		return this->moveTo(point.x, point.y);
	}

	VectorPath & lineTo(float x, float y);
	VectorPath & lineTo(const Vec2 &point) {
		return this->lineTo(point.x, point.y);
	}

	VectorPath & quadTo(float x1, float y1, float x2, float y2);
	VectorPath & quadTo(const Vec2& p1, const Vec2& p2) {
		return this->quadTo(p1.x, p1.y, p2.x, p2.y);
	}

	VectorPath & cubicTo(float x1, float y1, float x2, float y2, float x3, float y3);
	VectorPath & cubicTo(const Vec2& p1, const Vec2& p2, const Vec2& p3) {
		return this->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	}

	// use _to_rad user suffix to convert from degrees to radians
	VectorPath & arcTo(float rx, float ry, float rotation, bool largeFlag, bool sweepFlag, float x, float y);
	VectorPath & arcTo(const Vec2 & r, float rotation, bool largeFlag, bool sweepFlag, const Vec2 &target) {
		return this->arcTo(r.x, r.y, rotation, largeFlag, sweepFlag, target.x, target.y);
	}

	VectorPath & closePath();

	VectorPath & addRect(const Rect& rect);
	VectorPath & addOval(const Rect& oval);
	VectorPath & addCircle(float x, float y, float radius);
	VectorPath & addArc(const Rect& oval, float startAngleInRadians, float sweepAngleInRadians);

	VectorPath & setFillColor(const Color4B &color);
	const Color4B &getFillColor() const;

	VectorPath & setStrokeColor(const Color4B &color);
	const Color4B &getStrokeColor() const;

	VectorPath & setFillOpacity(uint8_t value);
	uint8_t getFillOpacity() const;

	VectorPath & setStrokeOpacity(uint8_t value);
	uint8_t getStrokeOpacity() const;

	VectorPath & setStrokeWidth(float width);
	float getStrokeWidth() const;

	VectorPath & setStyle(DrawStyle s);
	DrawStyle getStyle() const;

	VectorPath & setTransform(const Mat4 &);
	VectorPath & applyTransform(const Mat4 &);
	const Mat4 &getTransform() const;

	VectorPath & setAntialiased(bool value);
	bool isAntialiased() const;

	VectorPath & clear();

	StringView getId() const;

	bool empty() const;
	bool valid() const;

	operator bool() const;

	void setPath(Rc<Path> &&);
	Path *getPath() const;

	void markCopyOnWrite();
	void setImage(VectorImage *);

protected:
	void copy();

	bool _copyOnWrite = false;
	String _id;
	Rc<Path> _path;
	VectorImage *_image;
};

class VectorImageData : public Ref {
public:
	using Path = layout::Path;

	virtual ~VectorImageData() { }

	bool init(VectorImage *, Size size, Rect viewBox, Vector<layout::PathXRef> &&, Map<String, layout::Path> &&, uint16_t ids);
	bool init(VectorImage *, Size size, Rect viewBox);
	bool init(VectorImageData &);

	Size getImageSize() const { return _imageSize; }
	Rect getViewBox() const { return _viewBox; }
	const Map<String, Rc<Path>> &getPaths() const;

	Rc<Path> copyPath(StringView, const Rc<Path> &);

	uint16_t getNextId();

	Rc<Path> addPath(StringView, Path &&, Mat4 mat = Mat4::IDENTITY);
	void removePath(StringView);

	void clear();

	const Vector<layout::PathXRef> &getDrawOrder() const { return _order; }
	void setDrawOrder(Vector<layout::PathXRef> &&order) { _order = move(order); }
	void resetDrawOrder();

	void setViewBoxTransform(const Mat4 &m) { _viewBoxTransform = m; }
	const Mat4 &getViewBoxTransform() const { return _viewBoxTransform; }

	// general antialiasing settings used when paths drawn in batch
	void setAntialiased(bool value) { _isAntialiased = value; }
	bool isAntialiased() const { return _isAntialiased; }

	void setBatchDrawing(bool value) { _allowBatchDrawing = value; }
	bool isBatchDrawing() const { return _allowBatchDrawing; }

	template <typename Callback>
	void draw(const Callback &) const;

protected:
	bool _isAntialiased = true;
	bool _allowBatchDrawing = true;
	Size _imageSize;
	Rect _viewBox;
	Mat4 _viewBoxTransform = Mat4::IDENTITY;
	Vector<layout::PathXRef> _order;
	Map<String, Rc<Path>> _paths;
	uint16_t _nextId = 0;
	VectorImage *_image = nullptr;
};

class VectorImage : public Ref {
public:
	using Path = layout::Path;

	static bool isSvg(StringView);
	static bool isSvg(BytesView);
	static bool isSvg(FilePath);

	virtual ~VectorImage();

	bool init(Size, StringView);
	bool init(Size, Path &&);
	bool init(Size);
	bool init(StringView);
	bool init(BytesView);
	bool init(FilePath);

	Size getImageSize() const;
	Rect getViewBox() const;

	Rc<VectorPath> addPath(const Path &, StringView = StringView(), Mat4 = Mat4::IDENTITY);
	Rc<VectorPath> addPath(Path &&, StringView = StringView(), Mat4 = Mat4::IDENTITY);
	Rc<VectorPath> addPath(StringView = StringView(), Mat4 = Mat4::IDENTITY);

	Rc<VectorPath> getPath(StringView) const;
	const Map<String, Rc<VectorPath>> &getPaths() const { return _paths; }

	void removePath(const Rc<VectorPath> &);
	void removePath(StringView);

	void clear();

	const Vector<layout::PathXRef> &getDrawOrder() const;
	void setDrawOrder(const Vector<layout::PathXRef> &);
	void setDrawOrder(Vector<layout::PathXRef> &&);

	void resetDrawOrder();

	void setViewBoxTransform(const Mat4 &);
	const Mat4 &getViewBoxTransform() const;

	void setAntialiased(bool value);
	bool isAntialiased() const;

	void setBatchDrawing(bool value);
	bool isBatchDrawing() const;

	Rc<VectorImageData> popData();

	bool isDirty() const;
	void setDirty();
	void clearDirty();

protected:
	friend class VectorPath;

	void copy();
	void markCopyOnWrite();

	Rc<Path> copyPath(StringView, const Rc<Path> &);

	bool _dirty = false;
	bool _copyOnWrite = false;
	Rc<VectorImageData> _data;
	Map<String, Rc<VectorPath>> _paths;
};

template <typename Callback>
void VectorImageData::draw(const Callback &cb) const {
	for (auto &it : _order) {
		auto pathIt = _paths.find(it.id);
		if (pathIt != _paths.end()) {
			cb(*pathIt->second, it.mat);
		}
	}
}

}

#endif /* XENOLITH_NODES_VG_XLVECTORIMAGE_H_ */
