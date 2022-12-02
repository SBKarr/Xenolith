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


#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERENDERPASS_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERENDERPASS_H_

#include "XLRenderQueueAttachment.h"
#include "XLRenderQueueFrameQueue.h"

namespace stappler::xenolith::renderqueue {

class Pass : public NamedRef {
public:
	using FrameQueue = renderqueue::FrameQueue;
	using RenderOrdering = renderqueue::RenderOrdering;
	using PassHandle = renderqueue::PassHandle;
	using PassType = renderqueue::PassType;

	virtual ~Pass();

	virtual bool init(StringView, PassType, RenderOrdering, size_t subpassCount = 1);
	virtual void invalidate();

	virtual StringView getName() const override { return _name; }
	virtual RenderOrdering getOrdering() const { return _ordering; }
	virtual size_t getSubpassCount() const { return _subpassCount; }
	virtual PassType getType() const { return _type; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &);

	const Rc<FrameQueue> &getOwner() const { return _owner; }
	bool acquireForFrame(FrameQueue &, Function<void(bool)> &&onAcquired);
	bool releaseForFrame(FrameQueue &);

	const PassData *getData() const { return _data; }

	virtual Extent2 getSizeForFrame(const FrameQueue &) const;

	virtual const AttachmentDescriptor *getDescriptor(const Attachment *) const;

protected:
	friend class Queue;

	// called before compilation
	virtual void prepare(gl::Device &);

	size_t _subpassCount = 1;
	String _name;
	PassType _type = PassType::Graphics;
	RenderOrdering _ordering = RenderOrderingLowest;

	struct FrameQueueWaiter {
		Rc<FrameQueue> queue;
		Function<void(bool)> acquired;
	};

	Rc<FrameQueue> _owner;
	FrameQueueWaiter _next;
	Function<Extent2(const FrameQueue &)> _frameSizeCallback;
	const PassData *_data = nullptr;
};

class PassHandle : public NamedRef {
public:
	using Pass = renderqueue::Pass;
	using FrameHandle = renderqueue::FrameHandle;
	using FrameQueue = renderqueue::FrameQueue;
	using FrameSync = renderqueue::FrameSync;
	using RenderOrdering = renderqueue::RenderOrdering;

	virtual ~PassHandle();

	virtual bool init(Pass &, const FrameQueue &);
	virtual void setQueueData(FramePassData &);

	virtual StringView getName() const override;

	virtual const PassData *getData() const { return _data; }
	virtual const Rc<Pass> &getRenderPass() const { return _renderPass; }
	virtual const Rc<gl::Framebuffer> &getFramebuffer() const { return _queueData->framebuffer; }

	virtual bool isAvailable(const FrameQueue &) const;
	virtual bool isAsync() const { return _isAsync; }

	virtual bool isSubmitted() const;
	virtual bool isCompleted() const;

	virtual bool isFramebufferRequired() const;

	// Run data preparation process, that do not require queuing
	// returns true if 'prepare' completes immediately (either successful or not)
	// returns false if 'prepare' run some subroutines, and we should wait for them
	// To indicate success, call callback with 'true'. For failure - with 'false'
	// To indicate immediate failure, call callback with 'false', then return true;
	virtual bool prepare(FrameQueue &, Function<void(bool)> &&);

	// Run queue submission process
	// If submission were successful, you should call onSubmited callback with 'true'
	// If submission failed, call onSubmited with 'false'
	// If submission were successful, onComplete should be called when execution completes
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete);

	// after submit
	virtual void finalize(FrameQueue &, bool successful);

	virtual AttachmentHandle *getAttachmentHandle(const Attachment *) const;

	void autorelease(Ref *);

protected:
	bool _isAsync = false; // async passes can be submitted before previous frame submits all passes
	Rc<Pass> _renderPass;
	const PassData *_data = nullptr;
	FramePassData *_queueData = nullptr;
	Vector<Rc<Ref>> _autorelease;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERENDERPASS_H_ */
