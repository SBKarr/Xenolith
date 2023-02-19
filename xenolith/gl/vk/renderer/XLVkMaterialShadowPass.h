/**
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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMATERIALSHADOWPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMATERIALSHADOWPASS_H_

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"
#include "XLVkShadowRenderPass.h"
#include "XLVkMaterialVertexPass.h"

namespace stappler::xenolith::vk {

class MaterialShadowPass : public MaterialVertexPass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	static constexpr StringView ShadowPipeline = "ShadowPipeline";

	struct RenderQueueInfo {
		Application *app = nullptr;
		renderqueue::Queue::Builder *builder = nullptr;
		Extent2 extent;
		Function<void(gl::Resource::Builder &)> resourceCallback;
	};

	static bool makeDefaultRenderQueue(RenderQueueInfo &);

	virtual ~MaterialShadowPass() { }

	virtual bool init(StringView, RenderOrdering, size_t subpassCount = 1) override;

	const ShadowLightDataAttachment *getShadowData() const { return _shadowData; }
	const ShadowVertexAttachment *getShadowVertexBuffer() const { return _shadowVertexBuffer; }
	const ShadowTrianglesAttachment *getShadowTriangles() const { return _shadowTriangles; }
	const ShadowSdfImageAttachment *getSdf() const { return _sdf; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	// shadows
	const ShadowLightDataAttachment *_shadowData = nullptr;
	const ShadowVertexAttachment *_shadowVertexBuffer = nullptr;
	const ShadowTrianglesAttachment *_shadowTriangles = nullptr;
	const ShadowSdfImageAttachment *_sdf = nullptr;
};

class MaterialShadowPassHandle : public MaterialVertexPassHandle {
public:
	virtual ~MaterialShadowPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void prepareRenderPass(CommandBuffer &) override;
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &buf) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ShadowVertexAttachmentHandle *_shadowVertexBuffer = nullptr;
	const ShadowTrianglesAttachmentHandle *_shadowTriangles = nullptr;
	const ShadowSdfImageAttachmentHandle *_sdfImage = nullptr;
};

class MaterialShadowComputePass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	virtual ~MaterialShadowComputePass() { }

	virtual bool init(StringView, RenderOrdering);

	const ShadowLightDataAttachment *getLights() const { return _lights; }
	const ShadowVertexAttachment *getVertexes() const { return _vertexes; }
	const ShadowTrianglesAttachment *getTriangles() const { return _triangles; }
	const ShadowSdfImageAttachment *getSdf() const { return _sdf; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const ShadowLightDataAttachment *_lights = nullptr;
	const ShadowVertexAttachment *_vertexes = nullptr;
	const ShadowTrianglesAttachment *_triangles = nullptr;
	const ShadowSdfImageAttachment *_sdf = nullptr;
};

class MaterialShadowComputePassHandle : public QueuePassHandle {
public:
	virtual ~MaterialShadowComputePassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void writeShadowCommands(RenderPassImpl *, CommandBuffer &);
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	const ShadowLightDataAttachmentHandle *_lightsBuffer = nullptr;
	const ShadowVertexAttachmentHandle *_vertexBuffer = nullptr;
	const ShadowTrianglesAttachmentHandle *_trianglesBuffer = nullptr;
	const ShadowSdfImageAttachmentHandle *_sdfImage = nullptr;

	uint32_t _gridCellSize = 64;
};


}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALSHADOWPASS_H_ */
