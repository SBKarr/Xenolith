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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"

namespace stappler::xenolith::vk {

// this attachment should provide vertex & index buffers
class ShadowVertexAttachment : public BufferAttachment {
public:
	virtual ~ShadowVertexAttachment();

	virtual bool init(StringView, const gl::BufferInfo &);

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

// this attachment should provide vertex & index buffers
class ShadowTrianglesAttachment : public BufferAttachment {
public:
	struct TriangleData {
		Vec4 bb;
		Vec2 a;
		Vec2 b;
		Vec2 c;
		float value;
		float opacity;
	};

	struct IndexData {
		uint32_t a;
		uint32_t b;
		uint32_t c;
		uint32_t transform;
		float value;
		float opacity;
	};

	virtual ~ShadowTrianglesAttachment();

	virtual bool init(StringView, const gl::BufferInfo &);

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowImageAttachment : public ImageAttachment {
public:
	virtual ~ShadowImageAttachment();

	virtual bool init(StringView, Extent2 extent);

protected:
	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};


class ShadowPass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	static constexpr StringView SdfTrianglesComp = "SdfTrianglesComp";
	static constexpr StringView SdfImageComp = "SdfImageComp";

	struct RenderQueueInfo {
		Application *app = nullptr;
		renderqueue::Queue::Builder *builder = nullptr;
		Extent2 extent;
		Function<void(gl::Resource::Builder &)> resourceCallback;
	};

	static bool makeDefaultRenderQueue(renderqueue::Queue::Builder &, Extent2 extent);

	virtual ~ShadowPass() { }

	virtual bool init(StringView, RenderOrdering);

	const ShadowVertexAttachment *getVertexes() const { return _vertexes; }
	const ShadowTrianglesAttachment *getTriangles() const { return _triangles; }
	const ShadowImageAttachment *getImage() const { return _image; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const ShadowVertexAttachment *_vertexes = nullptr;
	const ShadowTrianglesAttachment *_triangles = nullptr;
	const ShadowImageAttachment *_image = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_ */
