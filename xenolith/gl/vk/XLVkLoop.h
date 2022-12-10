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

#ifndef XENOLITH_GL_VK_XLVKLOOP_H_
#define XENOLITH_GL_VK_XLVKLOOP_H_

#include "XLVk.h"
#include "XLVkInstance.h"
#include "XLVkSync.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::vk {

class Device;
class View;

class Loop : public gl::Loop {
public:
	struct Timer;
	struct Internal;

	virtual ~Loop();

	Loop();

	bool init(Application *, Rc<Instance> &&, uint32_t deviceIdx = gl::Instance::DefaultDevice);

	virtual void waitRinning() override;

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	virtual void cancel() override;

	virtual bool isRunning() const override { return _running.load(); }

	// virtual const Rc<gl::Device> &getDevice() const override;

	virtual void compileResource(Rc<gl::Resource> &&req, Function<void(bool)> && = nullptr) override;
	virtual void compileMaterials(Rc<gl::MaterialInputData> &&req, const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) override;
	virtual void compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> && = nullptr) override;
	virtual void compileImage(const Rc<gl::DynamicImage> &, Function<void(bool)> && = nullptr) override;

	// run frame with RenderQueue
	virtual void runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen = 0, Function<void(bool)> && = nullptr) override;

	// callback should return true to end spinning
	virtual void schedule(Function<bool(gl::Loop &)> &&, StringView) override;
	virtual void schedule(Function<bool(gl::Loop &)> &&, uint64_t, StringView) override;

	virtual void performInQueue(Rc<thread::Task> &&) override;
	virtual void performInQueue(Function<void()> &&func, Ref *target = nullptr) override;

	virtual void performOnGlThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false) override;
	virtual void performOnGlThread(Rc<thread::Task> &&) override;

	virtual bool isOnGlThread() const override;

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, uint64_t gen) override;

	virtual Rc<gl::Framebuffer> acquireFramebuffer(const PassData *, SpanView<Rc<gl::ImageView>>, Extent2 e) override;
	virtual void releaseFramebuffer(Rc<gl::Framebuffer> &&) override;

	virtual Rc<ImageStorage> acquireImage(const ImageAttachment *, const AttachmentHandle *, Extent3 e) override;
	virtual void releaseImage(Rc<ImageStorage> &&) override;

	virtual void addView(gl::ViewInfo &&) override;
	virtual void removeView(gl::View *) override;

	virtual Rc<gl::Semaphore> makeSemaphore() override;

	virtual const Vector<gl::ImageFormat> &getSupportedDepthStencilFormat() const override;

	virtual Rc<renderqueue::Queue> makeRenderFontQueue() const override;

	Rc<Fence> acquireFence(uint32_t, bool init = true);

	virtual void signalDependencies(const Vector<Rc<DependencyEvent>> &, bool success) override;
	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &, Function<void(bool)> &&) override;

protected:
	using gl::Loop::init;

	std::thread _thread;
	std::thread::id _threadId;

	std::mutex _mutex;
	std::condition_variable _cond;
	std::atomic<bool> _running = false;

	Internal *_internal = nullptr;
	uint32_t _deviceIndex = gl::Instance::DefaultDevice;

	Rc<Instance> _vkInstance;

	uint64_t _clock = 0;

};

}

#endif /* XENOLITH_GL_VK_XLVKLOOP_H_ */
