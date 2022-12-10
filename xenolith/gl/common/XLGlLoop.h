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

#ifndef XENOLITH_GL_COMMON_XLGLLOOP_H_
#define XENOLITH_GL_COMMON_XLGLLOOP_H_

#include "XLGl.h"
#include "XLApplication.h"
#include "XLGlMaterial.h"
#include "XLResourceCache.h"
#include "XLRenderQueueFrameCache.h"

namespace stappler::xenolith::gl {

class Loop : public thread::ThreadInterface<Interface> {
public:
	using FrameCache = renderqueue::FrameCache;
	using FrameRequest = renderqueue::FrameRequest;
	using ImageStorage = renderqueue::ImageStorage;
	using RenderQueue = renderqueue::Queue;
	using FrameHandle = renderqueue::FrameHandle;
	using PassData = renderqueue::PassData;
	using ImageAttachment = renderqueue::ImageAttachment;
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using DependencyEvent = renderqueue::DependencyEvent;

	static constexpr uint32_t LoopThreadId = 2;

	virtual ~Loop();

	Loop();

	virtual bool init(Application *app, Instance *gl);
	virtual void waitRinning() = 0;
	virtual void cancel() = 0;

	virtual bool isRunning() const = 0;

	const Rc<ResourceCache> &getResourceCache() const { return _resourceCache; }
	Application *getApplication() const { return _application; }
	const Rc<Instance> &getGlInstance() const { return _glInstance; }
	const Rc<FrameCache> &getFrameCache() const { return _frameCache; }

	virtual void compileResource(Rc<Resource> &&req, Function<void(bool)> &&) = 0;
	virtual void compileMaterials(Rc<MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) = 0;
	virtual void compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> && = nullptr) = 0;
	virtual void compileImage(const Rc<DynamicImage> &, Function<void(bool)> && = nullptr) = 0;

	// run frame with RenderQueue
	virtual void runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen = 0, Function<void(bool)> && = nullptr) = 0;

	// callback should return true to end spinning
	virtual void schedule(Function<bool(Loop &)> &&, StringView) = 0;
	virtual void schedule(Function<bool(Loop &)> &&, uint64_t, StringView) = 0;

	virtual void performInQueue(Rc<thread::Task> &&) = 0;
	virtual void performInQueue(Function<void()> &&func, Ref *target = nullptr) = 0;

	virtual void performOnGlThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false) = 0;
	virtual void performOnGlThread(Rc<thread::Task> &&) = 0;

	virtual bool isOnGlThread() const = 0;

	virtual Rc<renderqueue::FrameHandle> makeFrame(Rc<FrameRequest> &&, uint64_t gen) = 0;

	virtual Rc<Framebuffer> acquireFramebuffer(const PassData *, SpanView<Rc<ImageView>>, Extent2 e) = 0;
	virtual void releaseFramebuffer(Rc<Framebuffer> &&) = 0;

	virtual Rc<ImageStorage> acquireImage(const ImageAttachment *, const AttachmentHandle *, Extent3 e) = 0;
	virtual void releaseImage(Rc<ImageStorage> &&) = 0;

	virtual Rc<Semaphore> makeSemaphore() = 0;

	virtual void addView(ViewInfo &&) = 0;
	virtual void removeView(View *) = 0;

	virtual const Vector<gl::ImageFormat> &getSupportedDepthStencilFormat() const = 0;

	virtual Rc<renderqueue::Queue> makeRenderFontQueue() const = 0;

	virtual void signalDependencies(const Vector<Rc<DependencyEvent>> &, bool success) = 0;
	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &, Function<void(bool)> &&) = 0;

protected:
	std::atomic_flag _shouldExit;
	Rc<ResourceCache> _resourceCache;
	Application *_application = nullptr;
	Rc<Instance> _glInstance;
	Rc<FrameCache> _frameCache;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLLOOP_H_ */
