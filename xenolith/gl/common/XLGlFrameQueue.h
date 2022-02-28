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

#ifndef XENOLITH_GL_COMMON_XLGLFRAMEQUEUE_H_
#define XENOLITH_GL_COMMON_XLGLFRAMEQUEUE_H_

#include "XLGl.h"
#include <forward_list>

namespace stappler::xenolith::gl {

class FrameHandle;
class FrameCacheStorage;
class Loop;

enum class FrameRenderPassState {
	Initial,
	Ready,
	Owned,
	ResourcesAcquired,
	Prepared,
	Submission,
	Submitted,
	Complete,
	Finalized,
};

enum class FrameAttachmentState {
	Initial,
	Setup,
	InputRequired,
	Ready,
	ResourcesPending,
	ResourcesAcquired,
	Detached, // resource ownership transferred out of Frame
	Complete,
	ResourcesReleased,
	Finalized,
};

struct FrameQueueAttachmentData;
struct FrameQueueRenderPassData;

struct FrameQueueRenderPassData {
	FrameRenderPassState state = FrameRenderPassState::Initial;
	Rc<RenderPassHandle> handle;
	Extent2 extent;

	Vector<Pair<AttachmentDescriptor *, FrameQueueAttachmentData *>> attachments;
	HashMap<const Attachment *, FrameQueueAttachmentData *> attachmentMap;

	// Second value, FrameRenderPassState defines a state of required RenderPass, that is required to be reached
	// to define current RenderPass state as Ready
	// It is the latest state required for all Attachments to be ready for next RenderPass
	// Example:
	// Attachment1 can be used in RenderPass2 after Ready state of RenderPass1
	// 			(so, there is no actual dependency)
	// Attachment2 can be used in RenderPass2 after Submitted state of RenderPass1
	// 			(so, RenderPass2 can be started only after RenderPass1 is submitted, all synchronization done on GPU)
	// Attachment3 can be used in RenderPass2 after Complete state of RenderPass1
	// 			(so, there are some actions on CPU required to start RenderPass2 with results of RenderPass1)
	// Required state for RenderPass1 will be Complete (Ready < Submitted < Complete)
	Vector<Pair<FrameQueueRenderPassData *, FrameRenderPassState>> required;
	HashMap<FrameRenderPassState, Vector<FrameQueueRenderPassData *>> waiters;

	Rc<Framebuffer> framebuffer;
	bool waitForResult = false;
};

struct FrameQueueAttachmentData {
	FrameAttachmentState state = FrameAttachmentState::Initial;
	Rc<AttachmentHandle> handle;
	Extent3 extent;

	Vector<FrameQueueRenderPassData *> passes;

	// state of final RenderPass, on which Attachment resources can be released
	FrameRenderPassState final;

	Rc<ImageAttachmentObject> image;
	bool waitForResult = false;
};

struct FrameSyncAttachment {
	const AttachmentHandle *attachment;
	Rc<Semaphore> semaphore;
	PipelineStage stages = PipelineStage::None;
};

struct FrameSyncImage {
	const AttachmentHandle *attachment;
	ImageAttachmentObject *image = nullptr;
	AttachmentLayout newLayout = AttachmentLayout::Undefined;
};

struct FrameSync : public Ref {
	Vector<FrameSyncAttachment> waitAttachments;
	Vector<FrameSyncAttachment> signalAttachments;
	Vector<FrameSyncImage> images;
};

class FrameQueue final : public Ref {
public:
	virtual ~FrameQueue();

	bool init(const Rc<PoolRef> &, const Rc<RenderQueue> &, const Rc<FrameCacheStorage> &, FrameHandle &, Extent2);

	bool setup();
	void update();
	void invalidate();

	FrameHandle &getFrame() const { return *_frame; }
	Extent2 getExtent() const { return _extent; }
	const Rc<PoolRef> &getPool() const { return _pool; }
	Loop *getLoop() const;

	const HashMap<const RenderPassData *, FrameQueueRenderPassData> &getRenderPasses() const { return _renderPasses; }
	const HashMap<const Attachment *, FrameQueueAttachmentData> &getAttachments() const { return _attachments; }

	const FrameQueueAttachmentData *getAttachment(const Attachment *) const;
	const FrameQueueRenderPassData *getRenderPass(const RenderPassData *) const;

protected:
	void addRequiredPass(FrameQueueRenderPassData &, const FrameQueueRenderPassData &required,
			const FrameQueueAttachmentData &attachment, const AttachmentDescriptor &);

	bool isResourcePending(const FrameQueueAttachmentData &);
	void waitForResource(const FrameQueueAttachmentData &, Function<void()> &&);

	bool isResourcePending(const FrameQueueRenderPassData &);
	void waitForResource(const FrameQueueRenderPassData &, Function<void()> &&);

	void onAttachmentSetupComplete(FrameQueueAttachmentData &);
	void onAttachmentInput(FrameQueueAttachmentData &);
	void onAttachmentAcquire(FrameQueueAttachmentData &);
	void onAttachmentRelease(FrameQueueAttachmentData &);

	bool isRenderPassReady(const FrameQueueRenderPassData &) const;
	void updateRenderPassState(FrameQueueRenderPassData &, FrameRenderPassState);

	void onRenderPassReady(FrameQueueRenderPassData &);
	void onRenderPassOwned(FrameQueueRenderPassData &);
	void onRenderPassResourcesAcquired(FrameQueueRenderPassData &);
	void onRenderPassPrepared(FrameQueueRenderPassData &);
	void onRenderPassSubmission(FrameQueueRenderPassData &);
	void onRenderPassSubmitted(FrameQueueRenderPassData &);
	void onRenderPassComplete(FrameQueueRenderPassData &);

	Rc<FrameSync> makeRenderPassSync(FrameQueueRenderPassData &) const;
	PipelineStage getWaitStageForAttachment(FrameQueueRenderPassData &data, const gl::AttachmentHandle *handle) const;

	void onComplete();
	void onFinalized();

	void invalidate(FrameQueueAttachmentData &);
	void invalidate(FrameQueueRenderPassData &);

	void tryReleaseFrame();

	Rc<PoolRef> _pool;
	Rc<RenderQueue> _queue;
	Rc<FrameCacheStorage> _cache;
	Rc<FrameHandle> _frame;
	Loop *_loop = nullptr;
	Extent2 _extent;
	bool _finalized = false;
	bool _success = false;

	HashMap<const RenderPassData *, FrameQueueRenderPassData> _renderPasses;
	HashMap<const Attachment *, FrameQueueAttachmentData> _attachments;

	std::unordered_set<FrameQueueRenderPassData *> _renderPassesInitial;
	std::unordered_set<FrameQueueRenderPassData *> _renderPassesPrepared;
	std::unordered_set<FrameQueueAttachmentData *> _attachmentsInitial;

	std::forward_list<Rc<Ref>> _autorelease;
	uint32_t _renderPassSubmitted = 0;
	uint32_t _renderPassCompleted = 0;

	uint32_t _finalizedObjects = 0;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLFRAMEQUEUE_H_ */
