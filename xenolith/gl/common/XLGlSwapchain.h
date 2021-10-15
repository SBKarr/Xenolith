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

#ifndef XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_
#define XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_

#include "XLGlDevice.h"

namespace stappler::xenolith::gl {

class Swapchain : public Ref {
public:
	virtual ~Swapchain();

	virtual bool init(const View *, const Rc<RenderQueue> &);

	const View *getView() const { return _view; }

	virtual bool recreateSwapchain(Device &, gl::SwapchanCreationMode);
	virtual void invalidate(Device &);

	virtual Rc<gl::FrameHandle> beginFrame(gl::Loop &, bool force = false);
	virtual void setFrameSubmitted(Rc<gl::FrameHandle>);
	virtual void invalidateFrame(gl::FrameHandle &);
	virtual bool isFrameValid(const gl::FrameHandle &handle);

	// invalidate all frames in process before that
	virtual void incrementGeneration(AppEvent::Value);

	virtual bool isBestPresentMode() const { return true; }

	virtual bool isResetRequired();

	bool isValid() const { return _valid; }

	void setFrameTime(uint64_t v) { _frame = v; }
	uint64_t getFrameTime() const { return _frame; }

	void setFrameInterval(uint64_t v) { _frameInterval = v; }
	uint64_t getFrameInterval() const { return _frameInterval; }

protected:
	virtual Rc<FrameHandle> makeFrame(gl::Loop &, bool readyForSubmit) = 0;
	virtual bool canStartFrame() const;
	virtual bool scheduleNextFrame();

	uint64_t _order = 0;
	uint64_t _gen = 0;

	bool _valid = false;
	uint64_t _frame = 0;
	uint64_t _frameInterval = 1'000'000 / 60;
	uint64_t _suboptimal = 0;

	bool _nextFrameScheduled = false;
	std::deque<Rc<FrameHandle>> _frames;

	Device *_device = nullptr;
	const View *_view = nullptr;
	Rc<gl::RenderQueue> _renderQueue;
	Rc<gl::RenderQueue> _nextRenderQueue; // will be updated on reset
};

}

#endif /* XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_ */
