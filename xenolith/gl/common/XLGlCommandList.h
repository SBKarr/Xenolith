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

enum class CommandType : uint16_t {
	CommandGroup,
	VertexArray,
};

enum class CommandFlags : uint16_t {
	None,
	DoNotCount = 1 << 0
};

SP_DEFINE_ENUM_AS_MASK(CommandFlags)

struct DrawStateValues {
	renderqueue::DynamicState enabled = renderqueue::DynamicState::None;
	URect viewport;
	URect scissor;

	bool operator==(const DrawStateValues &) const = default;

	bool isScissorEnabled() const { return (enabled & renderqueue::DynamicState::Scissor) != renderqueue::DynamicState::None; }
	bool isViewportEnabled() const { return (enabled & renderqueue::DynamicState::Viewport) != renderqueue::DynamicState::None; }
};

struct CmdVertexArray {
	gl::MaterialId material = 0;
	gl::StateId state = 0;
	SpanView<Pair<Mat4, Rc<VertexData>>> vertexes;
	SpanView<int16_t> zPath;
	RenderingLevel renderingLevel = RenderingLevel::Solid;
};

struct Command {
	static Command *create(memory::pool_t *, CommandType t, CommandFlags = CommandFlags::None);

	void release();

	Command *next;
	CommandType type;
	CommandFlags flags = CommandFlags::None;
	void *data;
};

class CommandList : public gl::AttachmentInputData {
public:
	virtual ~CommandList();
	bool init(const Rc<PoolRef> &);

	template <typename Callback>
	void setStatCallback(Callback &&cb) {
		*_statCallback = std::forward<Callback>(cb);
	}

	void pushVertexArray(Rc<VertexData> &&, const Mat4 &, SpanView<int16_t> zPath, gl::MaterialId material,
			RenderingLevel, CommandFlags = CommandFlags::None);

	// data should be preallocated from frame's pool
	void pushVertexArray(SpanView<Pair<Mat4, Rc<VertexData>>>, SpanView<int16_t> zPath, gl::MaterialId material,
			RenderingLevel, CommandFlags = CommandFlags::None);

	gl::StateId addState(DrawStateValues);
	const DrawStateValues *getState(gl::StateId) const;

	void setCurrentState(gl::StateId state) { _currentState = state; }
	gl::StateId getCurrentState() const { return _currentState; }

	const Command *getFirst() const { return _first; }
	const Command *getLast() const { return _last; }

	void sendStat(const DrawStat &) const;

protected:
	void addCommand(Command *);

	Rc<PoolRef> _pool;
	gl::StateId _currentState = 0;
	Command *_first = nullptr;
	Command *_last = nullptr;
	memory::vector<DrawStateValues> *_states = nullptr;
	memory::function<void(DrawStat)> *_statCallback = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLCOMMANDLIST_H_ */
