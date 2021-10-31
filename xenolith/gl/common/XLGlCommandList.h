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

#ifndef XENOLITH_GL_COMMON_XLGLCOMMANDLIST_H_
#define XENOLITH_GL_COMMON_XLGLCOMMANDLIST_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

enum class CommandType {
	CommandGroup,
	VertexArray,
};

struct CmdVertexArray {
	Mat4 transform = Mat4::IDENTITY;
	gl::MaterialId material = 0;
	Rc<VertexData> vertexes;
	SpanView<int16_t> zPath;
};

struct Command {
	static Command *create(memory::pool_t *, CommandType t);

	Command *next;
	CommandType type;
	void *data;
};

class CommandList : public gl::AttachmentInputData {
public:
	bool init(const Rc<PoolRef> &);

	void pushVertexArray(const Rc<VertexData> &, const Mat4 &, SpanView<int16_t> zPath, gl::MaterialId material);

	const Command *getFirst() const { return _first; }
	const Command *getLast() const { return _last; }

protected:
	void addCommand(Command *);

	Rc<PoolRef> _pool;
	Command *_first = nullptr;
	Command *_last = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLCOMMANDLIST_H_ */
