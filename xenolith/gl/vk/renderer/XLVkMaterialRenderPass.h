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
#include "XLVkRenderPass.h"

namespace stappler::xenolith::vk {

// this attachment should provide material data buffer for rendering
class MaterialVertexAttachment : public gl::MaterialAttachment {
public:
	virtual ~MaterialVertexAttachment();

	virtual bool init(StringView, const gl::BufferInfo &, Vector<Rc<gl::Material>> && = Vector<Rc<gl::Material>>());

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameQueue &) override;
};

class MaterialVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~MaterialVertexAttachmentHandle();

	virtual bool init(const Rc<gl::Attachment> &, const gl::FrameQueue &);

	const Rc<gl::MaterialSet> &getMaterials() const { return _materials; }

	virtual bool isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

protected:
	Rc<gl::MaterialSet> _materials;
};

// this attachment should provide vertex & index buffers
class VertexMaterialAttachment : public BufferAttachment {
public:
	virtual ~VertexMaterialAttachment();

	virtual bool init(StringView, const gl::BufferInfo &, const MaterialVertexAttachment *);

	const MaterialVertexAttachment *getMaterials() const { return _materials; }

protected:
	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameQueue &) override;

	const MaterialVertexAttachment *_materials = nullptr;
};

class VertexMaterialAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~VertexMaterialAttachmentHandle();

	virtual bool setup(gl::FrameQueue &, Function<void(bool)> &&) override;

	virtual void submitInput(gl::FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

	const Vector<gl::VertexSpan> &getVertexData() const { return _spans; }
	const Rc<DeviceBuffer> &getVertexes() const { return _vertexes; }
	const Rc<DeviceBuffer> &getIndexes() const { return _indexes; }

	bool empty() const;

protected:
	virtual bool loadVertexes(gl::FrameHandle &, const Rc<gl::CommandList> &);

	virtual bool isGpuTransform() const { return false; }

	Rc<DeviceBuffer> _indexes;
	Rc<DeviceBuffer> _vertexes;
	Vector<gl::VertexSpan> _spans;

	const MaterialVertexAttachmentHandle *_materials = nullptr;
};

class MaterialRenderPass : public RenderPass {
public:
	virtual bool init(StringView, gl::RenderOrdering, size_t subpassCount = 1);

	const VertexMaterialAttachment *getVertexes() const { return _vertexes; }
	const MaterialVertexAttachment *getMaterials() const { return _materials; }

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(const gl::FrameQueue &) override;

protected:
	virtual void prepare(gl::Device &) override;

	const VertexMaterialAttachment *_vertexes = nullptr;
	const MaterialVertexAttachment *_materials = nullptr;
};

class MaterialRenderPassHandle : public RenderPassHandle {
public:
	virtual bool prepare(gl::FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &) override;
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, VkCommandBuffer &);

	virtual void doFinalizeTransfer(gl::MaterialSet * materials, VkCommandBuffer,
			Vector<VkImageMemoryBarrier> &outputImageBarriers, Vector<VkBufferMemoryBarrier> &outputBufferBarriers);

	const VertexMaterialAttachmentHandle *_vertexBuffer = nullptr;
	const MaterialVertexAttachmentHandle *_materialBuffer = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALRENDERPASS_H_ */
