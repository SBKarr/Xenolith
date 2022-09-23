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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEEMITTER_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEEMITTER_H_

#include "XLGl.h"
#include "XLRenderQueue.h"
#include "SPMovingAverage.h"

namespace stappler::xenolith::renderqueue {

class FrameRequest final : public Ref {
public:
	virtual ~FrameRequest();

	bool init(const Rc<FrameEmitter> &, Rc<ImageStorage> &&target);
	bool init(const Rc<Queue> &q);
	bool init(const Rc<Queue> &q, const Rc<FrameEmitter> &, Extent2);

	void addSignalDependency(Rc<DependencyEvent> &&);
	void addSignalDependencies(Vector<Rc<DependencyEvent>> &&);

	void addInput(const Attachment *, Rc<AttachmentInputData> &&);
	void acquireInput(Map<const Attachment *, Rc<AttachmentInputData>> &target);

	void setQueue(const Rc<Queue> &q);
	void setOutput(const Attachment *, Function<bool(const FrameAttachmentData &, bool)> &&);

	bool onOutputReady(gl::Loop &, FrameAttachmentData &) const;
	void onOutputInvalidated(gl::Loop &, FrameAttachmentData &) const;

	void finalize(gl::Loop &, bool success);

	bool bindSwapchainCallback(Function<bool(FrameAttachmentData &, bool)> &&);
	bool bindSwapchain(const Rc<gl::View> &);
	bool bindSwapchain(const Attachment *, const Rc<gl::View> &);

	bool isSwapchainAttachment(const Attachment *) const;

	const Rc<ImageStorage> & getRenderTarget() const { return _renderTarget; }

	const Rc<FrameEmitter> &getEmitter() const { return _emitter; }
	const Rc<Queue> &getQueue() const { return _queue; }

	Set<Rc<Queue>> getQueueList() const;

	Extent2 getExtent() const { return _extent; }
	float getDensity() const { return _density; }

	void setReadyForSubmit(bool value) { _readyForSubmit = value; }
	bool isReadyForSubmit() const { return _readyForSubmit; }

	bool isPersistentMapping() const { return _persistentMappings; }

	void setSceneId(uint32_t val) { _sceneId = val; }
	uint32_t getSceneId() const { return _sceneId; }

	const Rc<gl::View> &getSwapchain() const { return _swapchain; }

	const Vector<Rc<DependencyEvent>> &getSignalDependencies() const { return _signalDependencies; }

	FrameRequest() = default;

protected:
	FrameRequest(const FrameRequest &) = delete;
	FrameRequest &operator=(const FrameRequest &) = delete;

	Rc<FrameEmitter> _emitter;
	Rc<Queue> _queue;
	Extent2 _extent;
	float _density = 1.0f;
	Map<const Attachment *, Rc<AttachmentInputData>> _input;
	bool _readyForSubmit = true; // if true, do not wait synchronization with other active frames in emitter
	bool _persistentMappings = true; // true; // try to map per-frame GPU memory persistently
	uint32_t _sceneId = 0;

	Map<const Attachment *, Function<bool(FrameAttachmentData &, bool)>> _output;

	const Attachment *_swapchainAttachment = nullptr;
	Rc<gl::View> _swapchain;
	Rc<ImageStorage> _renderTarget;
	Rc<Ref> _swapchainHandle;

	Vector<Rc<DependencyEvent>> _signalDependencies;
};

// Frame emitter is an interface, that continuously spawns frames, and can control validity of a frame
class FrameEmitter : public Ref {
public:
	virtual ~FrameEmitter();

	virtual bool init(const Rc<gl::Loop> &, uint64_t frameInterval);
	virtual void invalidate();

	virtual void setFrameSubmitted(FrameHandle &);
	virtual bool isFrameValid(const FrameHandle &handle);

	virtual void acquireNextFrame();

	virtual void dropFrameTimeout();

	void dropFrames();

	bool isValid() const { return _valid; }

	void setFrameTime(uint64_t v) { _frame = v; }
	uint64_t getFrameTime() const { return _frame; }

	void setFrameInterval(uint64_t v) { _frameInterval = v; }
	uint64_t getFrameInterval() const { return _frameInterval; }

	const Rc<gl::Loop> &getLoop() const { return _loop; }

	uint64_t getLastFrameTime() const;
	uint64_t getAvgFrameTime() const;

	bool isReadyForSubmit() const;

	Rc<FrameRequest> makeRequest(Rc<ImageStorage> &&);
	Rc<FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

protected:
	virtual void onFrameEmitted(FrameHandle &);
	virtual void onFrameSubmitted(FrameHandle &);
	virtual void onFrameComplete(FrameHandle &);
	virtual void onFrameTimeout(uint64_t order);
	virtual void onFrameRequest(bool timeout);

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, bool readyForSubmit);
	virtual bool canStartFrame() const;
	virtual void scheduleNextFrame(Rc<FrameRequest> &&);
	virtual void scheduleFrameTimeout();

	virtual void enableCacheAttachments(const Rc<FrameHandle> &);

	uint64_t _submitted = 0;
	uint64_t _order = 0;
	uint64_t _gen = 0;

	bool _valid = true;
	std::atomic<uint64_t> _frame = 0;
	uint64_t _frameInterval = 1'000'000 / 60;
	uint64_t _suboptimal = 0;

	bool _frameTimeoutPassed = true;
	bool _nextFrameAcquired = false;
	bool _onDemand = true;
	Rc<FrameRequest> _nextFrameRequest;
	std::deque<Rc<FrameHandle>> _frames;
	std::deque<Rc<FrameHandle>> _framesPending;

	Rc<gl::Loop> _loop;

	uint64_t _lastSubmit = 0;

	mutable Mutex _frameTimeMutex;
	uint64_t _lastFrameTime = 0;
	math::MovingAverage<16, uint64_t> _avgFrameTime;

	uint64_t _lastTotalFrameTime = 0;

	Set<Rc<Queue>> _cacheRenderQueue;
	Set<gl::ImageInfoData> _cacheImages;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEEMITTER_H_ */
