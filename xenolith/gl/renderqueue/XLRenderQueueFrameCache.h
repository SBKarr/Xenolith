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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMECACHE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMECACHE_H_

#include "XLRenderQueue.h"

namespace stappler::xenolith::renderqueue {

struct FrameCacheFramebuffer final {
	Vector<Rc<gl::Framebuffer>> framebuffers;
	Extent2 extent;
};

struct FrameCacheImageAttachment final {
	uint32_t refCount;
	Vector<Rc<ImageStorage>> images;
};

class FrameCache final : public Ref {
public:
	using ImageInfoData = gl::ImageInfoData;

	virtual ~FrameCache();

	bool init(gl::Loop &, gl::Device &);
	void invalidate();

	Rc<gl::Framebuffer> acquireFramebuffer(const PassData *, SpanView<Rc<gl::ImageView>>, Extent2 e);
	void releaseFramebuffer(Rc<gl::Framebuffer> &&);

	Rc<ImageStorage> acquireImage(const gl::ImageInfo &, SpanView<gl::ImageViewInfo> v);
	void releaseImage(Rc<ImageStorage> &&);

	void addImage(const ImageInfoData &);
	void removeImage(const ImageInfoData &);

	void addImageView(uint64_t);
	void removeImageView(uint64_t);

	void addRenderPass(uint64_t);
	void removeRenderPass(uint64_t);

	void removeUnreachableFramebuffers();

	size_t getFramebuffersCount() const;
	size_t getImagesCount() const;
	size_t getImageViewsCount() const;

protected:
	bool isReachable(SpanView<uint64_t> ids) const;
	bool isReachable(const ImageInfoData &) const;

	void makeViews(const Rc<ImageStorage> &, SpanView<gl::ImageViewInfo>);

	gl::Loop *_loop = nullptr;
	gl::Device *_device = nullptr;
	Map<gl::ImageInfoData, FrameCacheImageAttachment> _images;
	Map<Vector<uint64_t>, FrameCacheFramebuffer> _framebuffers;
	Set<uint64_t> _imageViews;
	Set<uint64_t> _renderPasses;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEFRAMECACHE_H_ */
