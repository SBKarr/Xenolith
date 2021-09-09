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

#ifndef XENOLITH_GL_COMMON_XLGLDEVICE_H_
#define XENOLITH_GL_COMMON_XLGLDEVICE_H_

#include "XLGlRenderQueue.h"
#include "XLEventHeader.h"
#include <deque>

namespace stappler::xenolith::gl {

class Resource;
class Instance;
class View;
class RenderQueue;
class ObjectInterface;
class Loop;
class FrameHandle;

class Device : public Ref {
public:
	Device();
	virtual ~Device();

	virtual bool init(const Rc<Instance> &instance);

	virtual void begin(Application *, thread::TaskQueue &);
	virtual void end(thread::TaskQueue &);
	virtual void reset(thread::TaskQueue &);
	virtual void waitIdle();

	virtual Rc<gl::FrameHandle> beginFrame(gl::Loop &, gl::RenderQueue &);
	virtual void setFrameSubmitted(Rc<gl::FrameHandle>);
	virtual void invalidateFrame(gl::FrameHandle &);
	virtual bool isFrameValid(const gl::FrameHandle &handle);

	// invalidate all frames in process before that
	virtual void incrementGeneration();

	virtual Rc<Shader> makeShader(const ProgramData &) = 0;
	virtual Rc<Pipeline> makePipeline(const gl::RenderQueue &, const RenderPassData &, const PipelineData &) = 0;
	virtual Rc<RenderPassImpl> makeRenderPass(RenderPassData &) = 0;

	Rc<Shader> getProgram(StringView);
	Rc<Shader> addProgram(Rc<Shader>);

	virtual void compileResource(thread::TaskQueue &queue, const Rc<Resource> &req);
	virtual void compileRenderQueue(thread::TaskQueue &queue, const Rc<RenderQueue> &req);

	uint64_t getOrder() const { return _order; }

	virtual Rc<RenderQueue> createDefaultRenderQueue(const Rc<gl::Loop> &, thread::TaskQueue &, const gl::ImageInfo &);

	void addObject(ObjectInterface *);
	void removeObject(ObjectInterface *);

	virtual const Rc<gl::RenderQueue> getDefaultRenderQueue() const;

	virtual bool isBestPresentMode() const { return true; }

	size_t getFramesActive() const { return _frames.size(); }

protected:
	virtual Rc<FrameHandle> makeFrame(gl::Loop &, gl::RenderQueue &, bool readyForSubmit) = 0;
	virtual bool canStartFrame() const;
	virtual bool scheduleNextFrame();

	void clearShaders();
	void invalidateObjects();

	uint64_t _order = 0;
	uint32_t _gen = 0;
	Rc<Instance> _glInstance;
	Mutex _mutex;

	Map<String, Rc<Shader>> _shaders;

	std::unordered_set<ObjectInterface *> _objects;
	std::deque<Rc<FrameHandle>> _frames;
	bool _nextFrameScheduled = false;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLDEVICE_H_ */
