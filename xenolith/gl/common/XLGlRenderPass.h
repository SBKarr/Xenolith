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


#ifndef XENOLITH_GL_COMMON_XLGLRENDERPASS_H_
#define XENOLITH_GL_COMMON_XLGLRENDERPASS_H_

#include "XLGlAttachment.h"

namespace stappler::xenolith::gl {

class RenderPassHandle;
struct RenderPassData;

/** RenderOrdering defines order of execution for render passes between interdependent passes
 * if render passes is not interdependent, RenderOrdering can be used as an advice, or not used at all
 */
using RenderOrdering = ValueWrapper<uint32_t, class RenderOrderingFlag>;

static constexpr RenderOrdering RenderOrderingLowest = RenderOrdering::min();
static constexpr RenderOrdering RenderOrderingHighest = RenderOrdering::max();

class RenderPass : public NamedRef {
public:
	virtual ~RenderPass();

	virtual bool init(StringView, RenderPassType, RenderOrdering, size_t subpassCount = 1);
	virtual void invalidate();

	virtual StringView getName() const override { return _name; }
	virtual RenderOrdering getOrdering() const { return _ordering; }
	virtual size_t getSubpassCount() const { return _subpassCount; }
	virtual RenderPassType getType() const { return _type; }

	virtual Rc<RenderPassHandle> makeFrameHandle(const FrameQueue &);

	const Rc<gl::FrameQueue> &getOwner() const { return _owner; }
	bool acquireForFrame(gl::FrameQueue &, Function<void(bool)> &&onAcquired);
	bool releaseForFrame(gl::FrameQueue &);

	const RenderPassData *getData() const { return _data; }

	virtual Extent2 getSizeForFrame(const FrameQueue &) const;

protected:
	friend class RenderQueue;

	// called before compilation
	virtual void prepare(Device &);

	size_t _subpassCount = 1;
	String _name;
	RenderPassType _type = RenderPassType::Graphics;
	RenderOrdering _ordering = RenderOrderingLowest;

	struct FrameQueueWaiter {
		Rc<gl::FrameQueue> queue;
		Function<void(bool)> acquired;
	};

	Rc<gl::FrameQueue> _owner;
	FrameQueueWaiter _next;
	Function<Extent2(const FrameQueue &)> _frameSizeCallback;
	const RenderPassData *_data = nullptr;
};

class RenderPassHandle : public NamedRef {
public:
	virtual ~RenderPassHandle();

	virtual bool init(RenderPass &, const FrameQueue &);
	virtual void setQueueData(FrameQueueRenderPassData &);

	virtual StringView getName() const override;

	virtual const RenderPassData *getData() const { return _data; }
	virtual const Rc<RenderPass> &getRenderPass() const { return _renderPass; }
	virtual const Rc<Framebuffer> &getFramebuffer() const { return _queueData->framebuffer; }

	virtual bool isAvailable(const FrameQueue &) const;
	virtual bool isAsync() const { return _isAsync; }

	virtual bool isSubmitted() const;
	virtual bool isCompleted() const;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&);
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete);

	// after submit
	virtual void finalize(FrameQueue &, bool successful);

	virtual AttachmentHandle *getAttachmentHandle(const Attachment *) const;

protected:
	bool _isAsync = false; // async passes can be submitted before previous frame submits all passes
	Rc<RenderPass> _renderPass;
	const RenderPassData *_data = nullptr;
	FrameQueueRenderPassData *_queueData = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERPASS_H_ */
