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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMATERIALVERTEXPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMATERIALVERTEXPASS_H_

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"

namespace stappler::xenolith::vk {

class VertexMaterialAttachment : public BufferAttachment {
public:
	virtual ~VertexMaterialAttachment();

	virtual bool init(AttachmentBuilder &builder, const gl::BufferInfo &, const AttachmentData *);

	const AttachmentData *getMaterials() const { return _materials; }

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *_materials = nullptr;
};

class VertexMaterialAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~VertexMaterialAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) override;

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
	const MaterialAttachmentHandle *_materials = nullptr;
	mutable Rc<gl::CommandList> _commands;
	gl::DrawStat _drawStat;
};

class MaterialVertexPass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	static gl::ImageFormat select2dDepthFormat(SpanView<gl::ImageFormat> formats);
	static gl::ImageFormat select3dDepthFormat(SpanView<gl::ImageFormat> formats);

	virtual ~MaterialVertexPass() { }

	const AttachmentData *getVertexes() const { return _vertexes; }
	const AttachmentData *getMaterials() const { return _materials; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	const AttachmentData *_output = nullptr;
	const AttachmentData *_shadow = nullptr;
	const AttachmentData *_depth2d = nullptr;
	const AttachmentData *_depth3d = nullptr;

	const AttachmentData *_vertexes = nullptr;
	const AttachmentData *_materials = nullptr;
};

class MaterialVertexPassHandle : public QueuePassHandle {
public:
	virtual ~MaterialVertexPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void prepareRenderPass(CommandBuffer &);
	virtual void prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &);
	virtual void finalizeRenderPass(CommandBuffer &);

	const VertexMaterialAttachmentHandle *_vertexBuffer = nullptr;
	const MaterialAttachmentHandle *_materialBuffer = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALVERTEXPASS_H_ */
