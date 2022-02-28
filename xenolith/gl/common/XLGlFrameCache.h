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

#ifndef XENOLITH_GL_COMMON_XLGLFRAMECACHE_H_
#define XENOLITH_GL_COMMON_XLGLFRAMECACHE_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

class Loop;

struct FrameCacheRenderPass final {
	const RenderPassData *pass;
	Extent2 extent;
	std::multimap<uint64_t, Rc<Framebuffer>> framebuffers;
};

struct FrameCacheImageAttachment final {
	const ImageAttachment *attachment;
	Extent3 extent;
	Vector<Rc<ImageAttachmentObject>> images;
};

class FrameCacheStorage final : public Ref {
public:
	virtual ~FrameCacheStorage() { }

	bool init(Device *, const FrameEmitter *, const RenderQueue *);
	void invalidate();

	const RenderQueue *getQueue() const { return _queue; }

	void reset(const RenderPassData *, Extent2);
	Rc<Framebuffer> acquireFramebuffer(const Loop &, const RenderPassData *, SpanView<Rc<ImageView>>, Extent2 e);
	void releaseFramebuffer(const RenderPassData *, Rc<Framebuffer> &&);

	void reset(const ImageAttachment *, Extent3);
	Rc<ImageAttachmentObject> acquireImage(const Loop &, const ImageAttachment *, Extent3 e);
	void releaseImage(const ImageAttachment *, Rc<ImageAttachmentObject> &&);

	Rc<Semaphore> acquireSemaphore();

protected:
	Device *_device;
	const FrameEmitter *_emitter;
	const RenderQueue *_queue;

	Map<const RenderPassData *, FrameCacheRenderPass> _passes;
	Map<const ImageAttachment *, FrameCacheImageAttachment> _images;

	Mutex _invalidateMutex;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLFRAMECACHE_H_ */
