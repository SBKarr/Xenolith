/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEHANDLE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEHANDLE_H_

#include "XLRenderQueue.h"
#include "XLRenderQueueFrameEmitter.h"

namespace stappler::xenolith::renderqueue {

class FrameHandle : public Ref {
public:
	using FrameRequest = renderqueue::FrameRequest;

	static uint32_t GetActiveFramesCount();
	static void DescribeActiveFrames();

	virtual ~FrameHandle();

	bool init(gl::Loop &, gl::Device &, Rc<FrameRequest> &&, uint64_t gen);

	void update(bool init = false);

	uint64_t getTimeStart() const { return _timeStart; }
	uint64_t getTimeEnd() const { return _timeEnd; }
	uint64_t getOrder() const { return _order; }
	uint64_t getGen() const { return _gen; }
	uint64_t getSubmissionTime() const { return _submissionTime; }
	gl::Loop *getLoop() const { return _loop; }
	gl::Device *getDevice() const { return _device; }
	const Rc<FrameEmitter> &getEmitter() const { return _request->getEmitter(); }
	const Rc<Queue> &getQueue() const { return _request->getQueue(); }
	Extent2 getExtent() const { return _request->getExtent(); }
	const FrameAttachmentSpecialization &getFrameSpecialization() const { return _request->getFrameSpecialization(); }
	const Rc<PoolRef> &getPool() const { return _pool; }
	const Rc<gl::View> &getSwapchain() const { return _request->getSwapchain(); }

	gl::ImageInfoData getImageSpecialization(const ImageAttachment *) const;

	const Rc<ImageStorage> &getRenderTarget() const { return _request->getRenderTarget(); }
	const Vector<Rc<DependencyEvent>> &getSignalDependencies() const { return _request->getSignalDependencies(); }

	const Vector<Rc<FrameQueue>> &getFrameQueues() const { return _queues; }

	bool isSwapchainAttachment(const Attachment *) const;

	// spinners within frame should not spin directly on loop to preserve FrameHandle object
	virtual void schedule(Function<bool(FrameHandle &)> &&, StringView tag);

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performInQueue(Function<void(FrameHandle &)> &&, Ref *, StringView tag);
	virtual void performInQueue(Function<bool(FrameHandle &)> &&, Function<void(FrameHandle &, bool)> &&, Ref *, StringView tag);

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performOnGlThread(Function<void(FrameHandle &)> &&, Ref *, bool immediate, StringView tag);

	// required tasks should be completed before onComplete call
	virtual void performRequiredTask(Function<bool(FrameHandle &)> &&, Ref *, StringView tag);
	virtual void performRequiredTask(Function<bool(FrameHandle &)> &&, Function<void(FrameHandle &, bool)> &&, Ref *, StringView tag);

	virtual bool isSubmitted() const { return _submitted; }
	virtual bool isValid() const;
	virtual bool isValidFlag() const { return _valid; }

	virtual bool isPersistentMapping() const;

	virtual Rc<AttachmentInputData> getInputData(const Attachment *);

	virtual bool isReadyForSubmit() const { return _request->isReadyForSubmit(); }
	virtual void setReadyForSubmit(bool);

	virtual void invalidate();

	virtual void setCompleteCallback(Function<void(FrameHandle &)> &&);

	virtual void onQueueSubmitted(FrameQueue &);
	virtual void onQueueComplete(FrameQueue &);
	virtual void onQueueInvalidated(FrameQueue &);
	virtual void onOutputAttachment(FrameAttachmentData &);
	virtual void onOutputAttachmentInvalidated(FrameAttachmentData &);

	virtual void waitForDependencies(const Vector<Rc<DependencyEvent>> &, Function<void(FrameHandle &, bool)> &&);

	virtual void waitForInput(FrameQueue &queue, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb);

protected:
	virtual bool setup();

	virtual void onRequiredTaskCompleted(StringView tag);

	virtual void tryComplete();
	virtual void onComplete();

	gl::Loop *_loop = nullptr; // loop can not die until frames are performed
	gl::Device *_device = nullptr;
	Rc<PoolRef> _pool;
	Rc<FrameRequest> _request;

	uint64_t _timeStart = 0;
	uint64_t _timeEnd = 0;
	uint64_t _gen = 0;
	uint64_t _order = 0;
	uint64_t _submissionTime = 0;
	std::atomic<uint32_t> _tasksRequired = 0;
	uint32_t _tasksCompleted = 0;
	uint32_t _queuesSubmitted = 0;
	uint32_t _queuesCompleted = 0;

	bool _submitted = false;
	bool _completed = false;
	bool _valid = true;

	Vector<Rc<FrameQueue>> _queues;
	Function<void(FrameHandle &)> _complete;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUE */
