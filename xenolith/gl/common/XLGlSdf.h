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

#ifndef XENOLITH_GL_COMMON_XLGLSDF_H_
#define XENOLITH_GL_COMMON_XLGLSDF_H_

#include "XLGl.h"
#include "XLGlslSdfData.h"

namespace stappler::xenolith::gl {

enum class SdfShape {
	Circle2D,
	Rect2D,
	RoundedRect2D,
	Triangle2D,
	Polygon2D,
};

struct SdfPrimitive2D {
	Vec2 origin;
};

struct SdfCircle2D : SdfPrimitive2D {
	float radius;
};

struct SdfRect2D : SdfPrimitive2D {
	Size2 size;
};

struct SdfRoundedRect2D : SdfPrimitive2D {
	Size2 size;
	Vec4 radius;
};

struct SdfTriangle2D : SdfPrimitive2D {
	Vec2 a;
	Vec2 b;
	Vec2 c;
};

struct SdfPolygon2D {
	SpanView<Vec2> points;
};

struct SdfPrimitive2DHeader {
	SdfShape type;
	BytesView bytes;
};

struct CmdSdfGroup2D {
	Mat4 modelTransform;
	gl::StateId state = 0;
	float value = 0.0f;
	float opacity = 1.0f;

	memory::vector<SdfPrimitive2DHeader> data;

	void addCircle2D(Vec2 origin, float r);
	void addRect2D(Rect rect);
	void addRoundedRect2D(Rect rect, float r);
	void addRoundedRect2D(Rect rect, Vec4 r);
	void addTriangle2D(Vec2 origin, Vec2 a, Vec2 b, Vec2 c);
	void addPolygon2D(SpanView<Vec2>);
};

using Circle2DData = glsl::Circle2DData;
using Circle2DIndex = glsl::Circle2DIndex;
using Triangle2DData = glsl::Triangle2DData;
using Triangle2DIndex = glsl::Triangle2DIndex;
using Rect2DData = glsl::Rect2DData;
using Rect2DIndex = glsl::Rect2DIndex;

}


#endif /* XENOLITH_GL_COMMON_XLGLSDF_H_ */
