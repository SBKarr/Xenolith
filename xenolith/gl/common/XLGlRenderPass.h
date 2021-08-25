/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

/** RenderOrdering defines order of execution for render passes between interdependent passes
 * if render passes is not interdependent, RenderOrdering can be used as an advice, or not used at all
 */
using RenderOrdering = ValueWrapper<uint32_t, class RenderOrderingFlag>;

static constexpr RenderOrdering RenderOrderingLowest = RenderOrdering::min();
static constexpr RenderOrdering RenderOrderingHighest = RenderOrdering::max();

class RenderPass : public NamedRef {
public:
	virtual bool init(StringView, RenderOrdering, size_t subpassCount = 1);
	virtual void invalidate();

	virtual StringView getName() const override { return _name; }
	virtual RenderOrdering getOrdering() const { return _ordering; }
	virtual size_t getSubpassCount() const { return _subpassCount; }

	virtual Rc<RenderPassHandle> makeFrameHandle(RenderPassData *, const FrameHandle &);

protected:
	size_t _subpassCount = 1;
	String _name;
	RenderOrdering _ordering = RenderOrderingLowest;
};

class RenderPassHandle : public NamedRef {
public:
	virtual bool init(RenderPass &, RenderPassData *, const FrameHandle &);

	virtual StringView getName() const override;

	virtual void buildRequirements(const FrameHandle &, const Vector<Rc<RenderPassHandle>> &, const Vector<Rc<AttachmentHandle>> &);

	virtual bool isSubmitted() const { return _submitted; }
	virtual RenderPassData *getData() const { return _data; }

	virtual bool isReady() const;
	virtual bool isAsync() const { return _isAsync; }

	// if submit is true - do run + submit in one call
	virtual bool run(FrameHandle &);
	virtual void submit(FrameHandle &);

protected:
	bool _isAsync = false; // async passes can be submitted before previous frame submits all passes
	bool _submitted = false;
	Rc<RenderPass> _renderPass;
	RenderPassData *_data = nullptr;
	Vector<Rc<AttachmentHandle>> _attachments;
	Vector<Rc<RenderPassHandle>> _requiredPasses;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERPASS_H_ */
