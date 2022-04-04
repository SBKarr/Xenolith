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

#ifndef XENOLITH_NODES_VG_XLVECTORDRAW_H_
#define XENOLITH_NODES_VG_XLVECTORDRAW_H_

#include "XLDefine.h"

namespace stappler::xenolith {

enum class Winding {
	EvenOdd, // TESS_WINDING_ODD
	NonZero, // TESS_WINDING_NONZERO
	Positive, // TESS_WINDING_POSITIVE
	Negative, // TESS_WINDING_NEGATIVE
	AbsGeqTwo // TESS_WINDING_ABS_GEQ_TWO
};

enum class LineCup {
	Butt,
	Round,
	Square
};

enum class LineJoin {
	Miter,
	Round,
	Bevel
};

enum class DrawStyle {
	None = 0,
	Fill = 1,
	Stroke = 2,
	FillAndStroke = 3,
};

SP_DEFINE_ENUM_AS_MASK(DrawStyle)

struct VectorPathXRef {
	String id;
	Mat4 mat;
};

struct VectorLineDrawer {
	template <typename T>
	using Vector = memory::PoolInterface::VectorType<T>;

	VectorLineDrawer(memory::pool_t *);

	void setStyle(DrawStyle s, float e, float width);

	size_t capacity() const;
	void reserve(size_t);
	void clear();
	void force_clear();

	void drawLine(float x, float y);
	void drawQuadBezier(float x0, float y0, float x1, float y1, float x2, float y2);
	void drawCubicBezier(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
	void drawArc(float x0, float y0, float rx, float ry, float angle, bool largeArc, bool sweep, float x, float y);

	void pushLine(float x, float y);
	void pushOutline(float x, float y);
	void push(float x, float y);

	void pushLinePointWithIntersects(float x, float y);

	bool empty() const { return line.empty() && outline.empty(); }
	bool isStroke() const { return toInt(style & DrawStyle::Stroke); }
	bool isFill() const { return toInt(style & DrawStyle::Fill); }

	DrawStyle style = DrawStyle::None;

	float approxError = 0.0f;
	float distanceError = 0.0f;
	float angularError = 0.0f;

	Vector<Vec2> line; // verts on approximated line
	Vector<Vec2> outline; // verts on outline

	bool debug = false;
};

}

#endif /* XENOLITH_NODES_VG_XLVECTORDRAW_H_ */
