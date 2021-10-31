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

#include "XLGlCommandList.h"

namespace stappler::xenolith::gl {

Command *Command::create(memory::pool_t *p, CommandType t) {
	auto commandSize = sizeof(Command);

	switch (t) {
	case CommandType::CommandGroup: break;
	case CommandType::VertexArray: break;
	}

	auto bytes = memory::pool::palloc(p, commandSize);
	auto c = new (bytes) Command;
	c->next = nullptr;
	c->type = t;
	switch (t) {
	case CommandType::CommandGroup:
		c->data = nullptr;
		break;
	case CommandType::VertexArray:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdVertexArray)) ) CmdVertexArray;
		break;
	}
	return c;
}

bool CommandList::init(const Rc<PoolRef> &pool) {
	_pool = pool;
	return true;
}

void CommandList::pushVertexArray(const Rc<VertexData> &vert, const Mat4 &t, SpanView<int16_t> zPath, gl::MaterialId material) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray);
		auto cmdData = (CmdVertexArray *)cmd->data;
		cmdData->vertexes = vert;
		cmdData->transform = t;
		cmdData->zPath = zPath.pdup(_pool->getPool());
		cmdData->material = material;

		addCommand(cmd);
	});
}

void CommandList::addCommand(Command *cmd) {
	if (!_last) {
		_first = cmd;
	} else {
		_last->next = cmd;
	}
	_last = cmd;
}

/*static void appendToBuffer(memory::pool_t *p, DrawBuffer &vec, BytesView b) {
	// dirty hack with low-level stappler memory types to bypass safety validation
	auto origSize = vec.size();
	auto newSize = origSize + b.size();
	auto newmem = max(newSize, vec.capacity() * 2);
	auto alloc = DrawBuffer::allocator(p);
	vec.grow_alloc(alloc, newmem);
	memcpy(vec.data() + origSize, b.data(), b.size());
	vec.set_size(newSize);
}*/


}
