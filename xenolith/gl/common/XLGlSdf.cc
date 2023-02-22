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

#include "XLGlSdf.h"

namespace stappler::xenolith::gl {

void CmdSdfGroup2D::addCircle2D(Vec2 origin, float r) {
	auto p = data.get_allocator().getPool();

	auto circle = new (memory::pool::palloc(p, sizeof(SdfCircle2D))) SdfCircle2D;
	circle->origin = origin;
	circle->radius = r;

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Circle2D, BytesView((const uint8_t *)circle, sizeof(SdfCircle2D))});
}

void CmdSdfGroup2D::addRect2D(Rect r) {
	auto p = data.get_allocator().getPool();

	auto rect = new (memory::pool::palloc(p, sizeof(SdfRect2D))) SdfRect2D;
	rect->origin = Vec2(r.getMidX(), r.getMidY());
	rect->size = Size2(r.size / 2.0f);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Rect2D, BytesView((const uint8_t *)rect, sizeof(SdfRect2D))});
}

void CmdSdfGroup2D::addRoundedRect2D(Rect rect, float r) {
	auto p = data.get_allocator().getPool();

	auto roundedRect = new (memory::pool::palloc(p, sizeof(SdfRoundedRect2D))) SdfRoundedRect2D;
	roundedRect->origin = Vec2(rect.getMidX(), rect.getMidY());
	roundedRect->size = Size2(rect.size / 2.0f);
	roundedRect->radius = Vec4(r, r, r, r);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::RoundedRect2D, BytesView((const uint8_t *)roundedRect, sizeof(SdfRoundedRect2D))});
}

void CmdSdfGroup2D::addRoundedRect2D(Rect rect, Vec4 r) {
	auto p = data.get_allocator().getPool();

	auto roundedRect = new (memory::pool::palloc(p, sizeof(SdfRoundedRect2D))) SdfRoundedRect2D;
	roundedRect->origin = Vec2(rect.getMidX(), rect.getMidY());
	roundedRect->size = Size2(rect.size / 2.0f);
	roundedRect->radius = Vec4(r);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::RoundedRect2D, BytesView((const uint8_t *)roundedRect, sizeof(SdfRoundedRect2D))});
}

void CmdSdfGroup2D::addTriangle2D(Vec2 origin, Vec2 a, Vec2 b, Vec2 c) {
	auto p = data.get_allocator().getPool();

	auto triangle = new (memory::pool::palloc(p, sizeof(SdfTriangle2D))) SdfTriangle2D;
	triangle->origin = origin;
	triangle->a = a;
	triangle->b = b;
	triangle->c = c;

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Triangle2D, BytesView((const uint8_t *)triangle, sizeof(SdfTriangle2D))});
}

void CmdSdfGroup2D::addPolygon2D(SpanView<Vec2> view) {
	auto p = data.get_allocator().getPool();

	auto polygon = new (memory::pool::palloc(p, sizeof(SdfPolygon2D))) SdfPolygon2D;
	polygon->points = view.pdup(p);

	data.emplace_back(SdfPrimitive2DHeader{SdfShape::Polygon2D, BytesView((const uint8_t *)polygon, sizeof(SdfPolygon2D))});
}

}
