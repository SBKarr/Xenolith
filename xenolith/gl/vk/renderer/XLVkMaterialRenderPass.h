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

namespace stappler::xenolith::vk {

// this attachment should provide material data buffer for rendering
class MaterialVertexAttachment : public gl::MaterialAttachment {
public:
	virtual ~MaterialVertexAttachment();

	virtual bool init(StringView, const gl::BufferInfo &, Vector<Rc<gl::Material>> && = Vector<Rc<gl::Material>>());

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using gl::MaterialAttachment::init;
};

class MaterialVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~MaterialVertexAttachmentHandle();

	virtual bool init(const Rc<Attachment> &, const FrameQueue &) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

	const MaterialVertexAttachment *getMaterialAttachment() const;

	const Rc<gl::MaterialSet> getSet() const;

protected:
	std::mutex _mutex;
	mutable Rc<gl::MaterialSet> _materials;
};

// this attachment should provide vertex & index buffers
class VertexMaterialAttachment : public BufferAttachment {
public:
	virtual ~VertexMaterialAttachment();

	virtual bool init(StringView, const gl::BufferInfo &, const MaterialVertexAttachment *);

	const MaterialVertexAttachment *getMaterials() const { return _materials; }

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

	const MaterialVertexAttachment *_materials = nullptr;
};

class VertexMaterialAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~VertexMaterialAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

	const Vector<gl::VertexSpan> &getVertexData() const { return _spans; }
	const Rc<DeviceBuffer> &getVertexes() const { return _vertexes; }
	const Rc<DeviceBuffer> &getIndexes() const { return _indexes; }

	Rc<gl::CommandList> popCommands() const;

	bool empty() const;

protected:
	virtual bool loadVertexes(FrameHandle &, const Rc<gl::CommandList> &);

	virtual bool isGpuTransform() const { return false; }

	Rc<DeviceBuffer> _indexes;
	Rc<DeviceBuffer> _vertexes;
	Rc<DeviceBuffer> _transforms;
	Vector<gl::VertexSpan> _spans;

	Rc<gl::MaterialSet> _materialSet;
	const MaterialVertexAttachmentHandle *_materials = nullptr;
	mutable Rc<gl::CommandList> _commands;
	gl::DrawStat _drawStat;
};

class MaterialPass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	struct RenderQueueInfo {
		Application *app = nullptr;
		renderqueue::Queue::Builder *builder = nullptr;
		Extent2 extent;
		Function<void(gl::Resource::Builder &)> resourceCallback;
	};

	static bool makeDefaultRenderQueue(RenderQueueInfo &);

	virtual ~MaterialPass() { }

	virtual bool init(StringView, RenderOrdering, size_t subpassCount = 1);

	const VertexMaterialAttachment *getVertexes() const { return _vertexes; }
	const MaterialVertexAttachment *getMaterials() const { return _materials; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const VertexMaterialAttachment *_vertexes = nullptr;
	const MaterialVertexAttachment *_materials = nullptr;
};

class MaterialPassHandle : public QueuePassHandle {
public:
	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &) override;
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, VkCommandBuffer &);

	virtual void doFinalizeTransfer(gl::MaterialSet * materials, VkCommandBuffer,
			Vector<VkImageMemoryBarrier> &outputImageBarriers, Vector<VkBufferMemoryBarrier> &outputBufferBarriers);

	const VertexMaterialAttachmentHandle *_vertexBuffer = nullptr;
	const MaterialVertexAttachmentHandle *_materialBuffer = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALRENDERPASS_H_ */
