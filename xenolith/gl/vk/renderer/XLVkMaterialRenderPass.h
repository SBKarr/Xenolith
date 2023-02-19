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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMATERIALRENDERPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMATERIALRENDERPASS_H_

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"
#include "XLVkMaterialVertexPass.h"
#include "XLVkShadowRenderPass.h"

namespace stappler::xenolith::vk {

class MaterialPass : public MaterialVertexPass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	static constexpr StringView ShadowPipeline = "ShadowPipeline";
	static constexpr StringView ShadowPipelineNull = "ShadowPipelineNull";

	struct RenderQueueInfo {
		Application *app = nullptr;
		renderqueue::Queue::Builder *builder = nullptr;
		Extent2 extent;
		Function<void(gl::Resource::Builder &)> resourceCallback;
		bool withShadows = false;
	};

	static bool makeDefaultRenderQueue(RenderQueueInfo &);

	virtual ~MaterialPass() { }

	virtual bool init(StringView, RenderOrdering, size_t subpassCount = 1);

	const ShadowLightDataAttachment *getLights() const { return _lights; }
	const ShadowImageArrayAttachment *getArray() const { return _array; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const ShadowLightDataAttachment *_lights = nullptr;
	const ShadowImageArrayAttachment *_array = nullptr;
};

class MaterialPassHandle : public MaterialVertexPassHandle {
public:
	virtual ~MaterialPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void prepareRenderPass(CommandBuffer &) override;
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &) override;

	virtual void doFinalizeTransfer(gl::MaterialSet * materials,
			Vector<ImageMemoryBarrier> &outputImageBarriers, Vector<BufferMemoryBarrier> &outputBufferBarriers) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_lightsBuffer = nullptr;
	const ShadowImageArrayAttachmentHandle *_arrayImage = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALRENDERPASS_H_ */
