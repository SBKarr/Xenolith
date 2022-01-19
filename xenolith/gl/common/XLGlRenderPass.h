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

	virtual Rc<RenderPassHandle> makeFrameHandle(RenderPassData *, const FrameHandle &);

	const Rc<gl::FrameHandle> &getOwner() const { return _owner; }
	bool acquireForFrame(gl::FrameHandle &);
	bool releaseForFrame(gl::FrameHandle &);

	const RenderPassData *getData() const { return _data; }

protected:
	friend class RenderQueue;

	// called before compilation
	virtual void prepare(Device &);

	size_t _subpassCount = 1;
	String _name;
	RenderPassType _type = RenderPassType::Graphics;
	RenderOrdering _ordering = RenderOrderingLowest;

	Rc<gl::FrameHandle> _owner;
	Rc<gl::FrameHandle> _next;
	const RenderPassData *_data = nullptr;
};

class RenderPassHandle : public NamedRef {
public:
	virtual ~RenderPassHandle();

	virtual bool init(RenderPass &, RenderPassData *, const FrameHandle &);

	virtual StringView getName() const override;

	virtual void buildRequirements(const FrameHandle &, const Vector<Rc<RenderPassHandle>> &, const Vector<Rc<AttachmentHandle>> &);

	virtual RenderPassData *getData() const { return _data; }
	virtual const Rc<RenderPass> &getRenderPass() const { return _renderPass; }

	virtual bool isReady() const;
	virtual bool isAvailable(const FrameHandle &) const;
	virtual bool isAsync() const { return _isAsync; }

	virtual bool isSubmitted() const { return _submitted; }
	virtual void setSubmitted(bool value) { _submitted = value; }

	virtual bool isCompleted() const { return _completed; }
	virtual void setCompleted(bool value) { _completed = value; }

	// if submit is true - do run + submit in one call
	virtual bool prepare(FrameHandle &);
	virtual void submit(FrameHandle &, Function<void(const Rc<gl::RenderPassHandle> &)> &&);

	// after submit
	virtual void finalize(FrameHandle &, bool successful);

	virtual AttachmentHandle *getAttachmentHandle(const Attachment *) const;

protected:
	virtual void addRequiredAttachment(const Attachment *, const Rc<AttachmentHandle> &);

	bool _isAsync = false; // async passes can be submitted before previous frame submits all passes
	bool _submitted = false;
	bool _completed = false;
	Rc<RenderPass> _renderPass;
	RenderPassData *_data = nullptr;
	Map<const gl::Attachment *, Rc<AttachmentHandle>> _attachments;
	Vector<Rc<RenderPassHandle>> _requiredPasses;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERPASS_H_ */
