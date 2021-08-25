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

#ifndef XENOLITH_GL_COMMON_XLGLDRAWSCHEME_H_
#define XENOLITH_GL_COMMON_XLGLDRAWSCHEME_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

struct CommandGroup;

using DrawBuffer = memory::impl::mem_large<uint8_t, 0>;

class DrawScheme : public Ref {
public:
	static Rc<DrawScheme> create();
	static void destroy(DrawScheme *);

	void pushVertexArrayCmd(const Rc<Pipeline> &, const Rc<VertexData> &, const Mat4 &, SpanView<int16_t> zPath);

	void pushDrawIndexed(CommandGroup *g, Pipeline *, SpanView<Vertex_V4F_C4F_T2F> vertexes, SpanView<uint16_t> indexes);

	memory::pool_t *getPool() const { return pool; }

	DrawScheme(memory::pool_t *);

protected:
	memory::pool_t *pool = nullptr;
	CommandGroup *group = nullptr;

	DrawBuffer draw; // not binded
	DrawBuffer index; // index binding

	DrawBuffer data; // uniforms[0] : draw::DrawData
	DrawBuffer transforms; // uniforms[1] : layout::Mat4

	memory::map<VertexFormat, DrawBuffer> vertex; // buffer[N] : VertexFormat
};

}

#endif /* XENOLITH_GL_COMMON_XLGLDRAWSCHEME_H_ */
