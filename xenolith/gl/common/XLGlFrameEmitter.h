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

#ifndef XENOLITH_GL_COMMON_XLGLFRAMEEMITTER_H_
#define XENOLITH_GL_COMMON_XLGLFRAMEEMITTER_H_

#include "XLGlFrameCache.h"

namespace stappler::xenolith::gl {

class FrameHandle;
class Loop;
class Attachment;
class FrameEmitter;
class Swapchain;

struct FrameQueueAttachmentData;

class FrameRequest final : public Ref {
public:
	virtual ~FrameRequest();

	bool init(const Rc<RenderQueue> &q);
	bool init(const Rc<RenderQueue> &q, const Rc<FrameEmitter> &, Extent2);

	void setCacheInfo(const Rc<FrameEmitter> &, const Rc<FrameCacheStorage> &, bool readyForSubmit);

	void addInput(const Attachment *, Rc<AttachmentInputData> &&);
	void acquireInput(Map<const Attachment *, Rc<AttachmentInputData>> &target);

	bool onOutputReady(gl::Loop &, FrameQueueAttachmentData &) const;

	void finalize();

	bool bindSwapchain(const Rc<Swapchain> &);
	bool bindSwapchain(const Attachment *, const Rc<Swapchain> &);

	bool isSwapchainAttachment(const Attachment *) const;

	Rc<ImageAttachmentObject> acquireSwapchainImage(const Loop &loop, const ImageAttachment *a, Extent3 e);

	const Rc<FrameEmitter> &getEmitter() const { return _emitter; }
	const Rc<RenderQueue> &getQueue() const { return _queue; }
	const Rc<FrameCacheStorage> &getCache() const { return _cache; }

	Extent2 getExtent() const { return _extent; }

	void setReadyForSubmit(bool value) { _readyForSubmit = value; }
	bool isReadyForSubmit() const { return _readyForSubmit; }

	bool isPersistentMapping() const { return _persistentMappings; }

	void setSceneId(uint32_t val) { _sceneId = val; }
	uint32_t getSceneId() const { return _sceneId; }

	FrameRequest() = default;

protected:
	FrameRequest(const FrameRequest &) = delete;
	FrameRequest &operator=(const FrameRequest &) = delete;

	Rc<FrameEmitter> _emitter;
	Rc<RenderQueue> _queue;
	Rc<FrameCacheStorage> _cache;
	Extent2 _extent;
	Map<const Attachment *, Rc<AttachmentInputData>> _input;
	bool _readyForSubmit = true; // if true, do not wait synchronization with other active frames in emitter
	bool _persistentMappings = true; // try to map per-frame GPU memory persistently
	uint32_t _sceneId = 0;

	// return true to setaside output
	Map<const Attachment *, Function<bool(const Rc<FrameCacheStorage> &, const FrameQueueAttachmentData &)>> _output;

	const Attachment *_swapchainAttachment = nullptr;
	Rc<Swapchain> _swapchain;
};

// Frame emitter is an interface, that continuously spawns frames, and can control validity of a frame
class FrameEmitter : public Ref {
public:
	virtual ~FrameEmitter();

	virtual bool init(const Rc<Loop> &, uint64_t frameInterval);
	virtual void invalidate();

	virtual void setFrameSubmitted(gl::FrameHandle &);
	virtual bool isFrameValid(const gl::FrameHandle &handle);

	virtual void removeCacheStorage(const FrameCacheStorage *);

	virtual void acquireNextFrame();

	virtual void dropFrameTimeout();

	bool isValid() const { return _valid; }

	void setFrameTime(uint64_t v) { _frame = v; }
	uint64_t getFrameTime() const { return _frame; }

	void setFrameInterval(uint64_t v) { _frameInterval = v; }
	uint64_t getFrameInterval() const { return _frameInterval; }

	const Rc<Loop> &getLoop() const { return _loop; }

protected:
	virtual void onFrameEmitted(gl::FrameHandle &);
	virtual void onFrameSubmitted(gl::FrameHandle &);
	virtual void onFrameComplete(gl::FrameHandle &);
	virtual void onFrameTimeout(uint64_t order);
	virtual void onFrameRequest(bool timeout);

	virtual Rc<FrameHandle> makeFrame(Rc<FrameRequest> &&, bool readyForSubmit);
	virtual bool canStartFrame() const;
	virtual void scheduleNextFrame(Rc<FrameRequest> &&);
	virtual void scheduleFrameTimeout();
	virtual Rc<gl::FrameHandle> submitNextFrame(Rc<FrameRequest> &&);

	uint64_t _submitted = 0;
	uint64_t _order = 0;
	uint64_t _gen = 0;

	bool _valid = true;
	uint64_t _frame = 0;
	uint64_t _frameInterval = 1'000'000 / 60;
	uint64_t _suboptimal = 0;

	bool _frameTimeoutPassed = true;
	bool _nextFrameAcquired = false;
	Rc<FrameRequest> _nextFrameRequest;
	std::deque<Rc<FrameHandle>> _frames;

	Rc<Loop> _loop;
	Map<const RenderQueue *, Rc<FrameCacheStorage>> _frameCache;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLFRAMEEMITTER_H_ */
