/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_CORE_BASE_XLVERTEXARRAY_H_
#define XENOLITH_CORE_BASE_XLVERTEXARRAY_H_

#include "XLDefine.h"
#include "XLFontStyle.h"

namespace stappler::xenolith {

class VertexArray : public Ref {
public:
	struct Quad {
		// 0 - 2
		// |   |
		// 1 - 3
		SpanView<gl::Vertex_V4F_V4F_T2F2U> vertexes; // tl bl tr br
		SpanView<uint32_t> indexes; // 0 1 2 3 2 1
		size_t firstVertex;
		size_t firstIndex;

		Quad & setTextureRect(const Rect &, float texWidth, float texHeight, bool flippedX, bool flippedY, bool rotated = false);
		Quad & setTexturePoints(const Vec2 &tl, const Vec2 &bl, const Vec2 &tr, const Vec2 &br, float texWidth, float texHeight);

		// Vec4 - you can pass whatever you want as w component to shader
		Quad & setGeometry(const Vec4 &origin, const Size2 &size, const Mat4 &t);
		Quad & setGeometry(const Vec4 &origin, const Size2 &size);
		Quad & setColor(const Color4F &color);
		Quad & setColor(SpanView<Color4F>); // tl bl tr br
		Quad & setColor(std::initializer_list<Color4F> &&); // tl bl tr br

		Quad & drawChar(const font::Metrics &m, const font::CharLayout &l, int16_t charX, int16_t charY,
				const Color4B &color, font::TextDecoration, uint16_t face);
		Quad & drawUnderlineRect(int16_t charX, int16_t charY, uint16_t width, uint16_t height, const Color4B &color);
	};

	virtual ~VertexArray();

	bool init(uint32_t bufferCapacity, uint32_t indexCapacity);
	bool init(const Rc<gl::VertexData> &);

	void reserve(uint32_t bufferCapacity, uint32_t indexCapacity);

	Rc<gl::VertexData> pop();
	Rc<gl::VertexData> dup();

	bool empty() const;
	void clear();

	// Quad is not valid after any other modifications, do not store this struct
	// Use firstVertex and firstIndex to reacquire this quad when needed
	Quad addQuad();
	Quad getQuad(size_t firstVertex, size_t firstIndex);

	void updateColor(const Color4F &color);
	void updateColor(const Color4F &color, const Vector<ColorMask> &);
	void updateColorQuads(const Color4F &color, const Vector<ColorMask> &);

	size_t getVertexCount() const;
	size_t getIndexCount() const;

protected:
	void copy();

	bool _copyOnWrite = false;
	Rc<gl::VertexData> _data;
};

}
#endif /* XENOLITH_CORE_BASE_XLVERTEXARRAY_H_ */
