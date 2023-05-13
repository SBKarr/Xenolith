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
	using Builder = renderqueue::Queue::Builder;

	static constexpr StringView ShadowPipeline = "ShadowPipeline";

	enum class Flags {
		None = 0,
		Render3D = 1 << 0,
	};

	struct RenderQueueInfo {
		gl::Loop *target = nullptr;
		Extent2 extent;
		Flags flags = Flags::None;
		Function<void(gl::Resource::Builder &)> resourceCallback;
	};

	struct PassCreateInfo {
		gl::Loop *target = nullptr;
		Extent2 extent;
		Flags flags;

		const AttachmentData *shadowSdfAttachment = nullptr;
		const AttachmentData *lightsAttachment = nullptr;
		const AttachmentData *sdfPrimitivesAttachment = nullptr;
	};

	static bool makeDefaultRenderQueue(Builder &, RenderQueueInfo &);

	virtual ~MaterialShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, PassBuilder &passBuilder, const PassCreateInfo &info);

	const AttachmentData *getLightsData() const { return _lightsData; }
	const AttachmentData *getShadowPrimitives() const { return _shadowPrimitives; }
	const AttachmentData *getSdf() const { return _sdf; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	Flags _flags = Flags::None;

	// shadows
	const AttachmentData *_lightsData = nullptr;
	const AttachmentData *_shadowPrimitives = nullptr;
	const AttachmentData *_sdf = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(MaterialShadowPass::Flags)

class MaterialShadowPassHandle : public MaterialVertexPassHandle {
public:
	virtual ~MaterialShadowPassHandle() { }

	virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

protected:
	virtual void prepareRenderPass(CommandBuffer &) override;
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &buf) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ShadowPrimitivesAttachmentHandle *_shadowPrimitives = nullptr;
	const ImageAttachmentHandle *_sdfImage = nullptr;
};

class MaterialComputeShadowPass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using Builder = renderqueue::Queue::Builder;

	static constexpr StringView SdfTrianglesComp = "SdfTrianglesComp";
	static constexpr StringView SdfCirclesComp = "SdfCirclesComp";
	static constexpr StringView SdfRectsComp = "SdfRectsComp";
	static constexpr StringView SdfRoundedRectsComp = "SdfRoundedRectsComp";
	static constexpr StringView SdfPolygonsComp = "SdfPolygonsComp";
	static constexpr StringView SdfImageComp = "SdfImageComp";

	virtual ~MaterialComputeShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, PassBuilder &passBuilder, Extent2 defaultExtent);

	const AttachmentData *getLights() const { return _lights; }
	const AttachmentData *getVertexes() const { return _vertexes; }
	const AttachmentData *getPrimitives() const { return _primitives; }
	const AttachmentData *getSdf() const { return _sdf; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	const AttachmentData *_lights = nullptr;
	const AttachmentData *_vertexes = nullptr;
	const AttachmentData *_primitives = nullptr;
	const AttachmentData *_sdf = nullptr;
};

class MaterialComputeShadowPassHandle : public QueuePassHandle {
public:
	virtual ~MaterialComputeShadowPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void writeShadowCommands(RenderPassImpl *, CommandBuffer &);
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	const ShadowLightDataAttachmentHandle *_lightsBuffer = nullptr;
	const ShadowVertexAttachmentHandle *_vertexBuffer = nullptr;
	const ShadowPrimitivesAttachmentHandle *_primitivesBuffer = nullptr;
	const ShadowSdfImageAttachmentHandle *_sdfImage = nullptr;

	uint32_t _gridCellSize = 64;
};


}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALSHADOWPASS_H_ */
