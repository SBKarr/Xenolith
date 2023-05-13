/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEQUEUE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEQUEUE_H_

#include "XLRenderQueue.h"
#include <forward_list>

namespace stappler::xenolith::renderqueue {

struct FramePassDataRequired {
	FramePassData *data;
	FrameRenderPassState requiredState;
	FrameRenderPassState lockedState;

	FramePassDataRequired() = default;
	FramePassDataRequired(FramePassData *data, FrameRenderPassState required, FrameRenderPassState locked)
	: data(data), requiredState(required), lockedState(locked) { }
};

struct FramePassData {
	FrameRenderPassState state = FrameRenderPassState::Initial;
	Rc<PassHandle> handle;
	Extent2 extent;

	Vector<Pair<const AttachmentPassData *, FrameAttachmentData *>> attachments;
	HashMap<const AttachmentData *, FrameAttachmentData *> attachmentMap;

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
	Vector<FramePassDataRequired> required;
	HashMap<FrameRenderPassState, Vector<FramePassData *>> waiters;

	Rc<gl::Framebuffer> framebuffer;
	bool waitForResult = false;

	uint64_t submitTime = 0;
};

struct FrameAttachmentData {
	FrameAttachmentState state = FrameAttachmentState::Initial;
	Rc<AttachmentHandle> handle;
	Extent3 extent;

	Vector<FramePassData *> passes;

	// state of final RenderPass, on which Attachment resources can be released
	FrameRenderPassState final;

	Rc<ImageStorage> image;
	bool waitForResult = false;
};

struct FrameSyncAttachment {
	const AttachmentHandle *attachment;
	Rc<gl::Semaphore> semaphore;
	ImageStorage *image = nullptr;
	PipelineStage stages = PipelineStage::None;
};

struct FrameSyncImage {
	const AttachmentHandle *attachment;
	ImageStorage *image = nullptr;
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

	bool init(const Rc<PoolRef> &, const Rc<Queue> &, FrameHandle &, Extent2);

	bool setup();
	void update();
	void invalidate();

	bool isFinalized() const { return _finalized; }

	const Rc<FrameHandle> &getFrame() const { return _frame; }
	Extent2 getExtent() const { return _extent; }
	const Rc<PoolRef> &getPool() const { return _pool; }
	const Rc<Queue> &getRenderQueue() const { return _queue; }
	gl::Loop *getLoop() const;

	const HashMap<const PassData *, FramePassData> &getRenderPasses() const { return _renderPasses; }
	const HashMap<const AttachmentData *, FrameAttachmentData> &getAttachments() const { return _attachments; }
	uint64_t getSubmissionTime() const { return _submissionTime; }

	const FrameAttachmentData *getAttachment(const AttachmentData *) const;
	const FramePassData *getRenderPass(const PassData *) const;

protected:
	void addRequiredPass(FramePassData &, const FramePassData &required,
			const FrameAttachmentData &attachment, const AttachmentPassData &);

	bool isResourcePending(const FrameAttachmentData &);
	void waitForResource(const FrameAttachmentData &, Function<void(bool)> &&);

	bool isResourcePending(const FramePassData &);
	void waitForResource(const FramePassData &, Function<void()> &&);

	void onAttachmentSetupComplete(FrameAttachmentData &);
	void onAttachmentInput(FrameAttachmentData &);
	void onAttachmentAcquire(FrameAttachmentData &);
	void onAttachmentRelease(FrameAttachmentData &);

	bool isRenderPassReady(const FramePassData &) const;
	bool isRenderPassReadyForState(const FramePassData &, FrameRenderPassState) const;
	void updateRenderPassState(FramePassData &, FrameRenderPassState);

	void onRenderPassReady(FramePassData &);
	void onRenderPassOwned(FramePassData &);
	void onRenderPassResourcesAcquired(FramePassData &);
	void onRenderPassPrepared(FramePassData &);
	void onRenderPassSubmission(FramePassData &);
	void onRenderPassSubmitted(FramePassData &);
	void onRenderPassComplete(FramePassData &);

	Rc<FrameSync> makeRenderPassSync(FramePassData &) const;
	PipelineStage getWaitStageForAttachment(FramePassData &data, const AttachmentHandle *handle) const;

	void onComplete();
	void onFinalized();

	void invalidate(FrameAttachmentData &);
	void invalidate(FramePassData &);

	void tryReleaseFrame();

	void finalizeAttachment(FrameAttachmentData &);

	Rc<PoolRef> _pool;
	Rc<Queue> _queue;
	Rc<FrameHandle> _frame;
	gl::Loop *_loop = nullptr;
	Extent2 _extent;
	uint64_t _order = 0;
	bool _finalized = false;
	bool _success = false;

	HashMap<const PassData *, FramePassData> _renderPasses;
	HashMap<const AttachmentData *, FrameAttachmentData> _attachments;

	std::unordered_set<FramePassData *> _renderPassesInitial;
	std::unordered_set<FramePassData *> _renderPassesPrepared;
	std::unordered_set<FrameAttachmentData *> _attachmentsInitial;

	std::forward_list<Rc<Ref>> _autorelease;
	uint32_t _renderPassSubmitted = 0;
	uint32_t _renderPassCompleted = 0;

	uint32_t _finalizedObjects = 0;
	uint64_t _submissionTime = 0;

	Vector<Pair<FramePassData *, FrameRenderPassState>> _awaitPasses;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMEQUEUE_H_ */
