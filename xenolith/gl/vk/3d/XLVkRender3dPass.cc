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

#include "XLVkRender3dPass.h"
#include "XLVkAttachment.h"
#include "XLVkMaterialVertexPass.h"
#include "XLVkRenderPassImpl.h"

namespace stappler::xenolith::vk {

struct Render3dMeshIndexData {
	Buffer *indexBuffer;
	uint32_t instanceCount;
	uint32_t instanceOffset;
	uint32_t indexCount;
	uint32_t indexOffset;
	uint32_t materialIdx;
	uint32_t vertexBufferIdx;
};

class Render3dVertexAttachment : public BufferAttachment {
public:
	virtual ~Render3dVertexAttachment() { }

protected:
};

class Render3dVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~Render3dVertexAttachmentHandle() { }

	SpanView<Render3dMeshIndexData> getIndexes() const;

protected:
	Vector<Render3dMeshIndexData> _indexes;
};

class Render3dInstanceAttachment : public BufferAttachment {
public:
	virtual ~Render3dInstanceAttachment() { }

protected:
};

class Render3dInstanceAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~Render3dInstanceAttachmentHandle() { }

protected:
};

void Render3dPass::makeDefaultPass(Builder &builder, const PassCreateInfo &info) {
	using namespace renderqueue;

	// depth buffer - temporary/transient
	auto depth = Rc<vk::ImageAttachment>::create("3dDepth",
		gl::ImageInfo(
			info.extent,
			gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			MaterialVertexPass::selectDepthFormat(info.app->getGlLoop()->getSupportedDepthStencilFormat())),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F::WHITE
	});

	auto renderPass = Rc<Render3dPass>::create("Render3dPass", RenderOrdering(0));
	builder.addRenderPass(renderPass);

	auto materialInput = Rc<vk::MaterialAttachment>::create("Render3dMaterialInput",
		gl::BufferInfo(gl::BufferUsage::StorageBuffer));

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::Render3dVertexAttachment>::create("Render3dVertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);

	// Vertex input attachment - per-frame vertex list
	auto instanceInput = Rc<vk::Render3dInstanceAttachment>::create("Render3dInstanceInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer));

	builder.addPassInput(renderPass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	builder.addPassInput(renderPass, 0, instanceInput, AttachmentDependencyInfo()); // 1
	builder.addPassInput(renderPass, 0, materialInput, AttachmentDependencyInfo()); // 2

	builder.addPassOutput(renderPass, 0, info.outputAttachment, AttachmentDependencyInfo{
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,
		FrameRenderPassState::Submitted,
	}, DescriptorType::Attachment, AttachmentLayout::Ignored);

	builder.addPassDepthStencil(renderPass, 0, depth, AttachmentDependencyInfo{
		PipelineStage::EarlyFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		PipelineStage::LateFragmentTest, AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		FrameRenderPassState::Submitted,
	});

	builder.addInput(vertexInput);
}

bool Render3dPass::init(StringView name, RenderOrdering ordering) {
	return QueuePass::init(name, PassType::Graphics, ordering, 1);
}

Rc<Render3dPass::PassHandle> Render3dPass::makeFrameHandle(const FrameQueue &queue) {
	return Rc<Render3dPassHandle>::create(this, queue);
}

void Render3dPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<MaterialAttachment *>(it->getAttachment())) {
			_materials = a;
		} else if (auto a = dynamic_cast<Render3dVertexAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

bool Render3dPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (Render3dPass *)_renderPass.get();

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materials = (const MaterialAttachmentHandle *)materialBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexes = (const Render3dVertexAttachmentHandle *)vertexBuffer->handle.get();
	}


	return QueuePassHandle::prepare(q, move(cb));
}

Vector<const CommandBuffer *> Render3dPassHandle::doPrepareCommands(FrameHandle &) {
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		auto materials = _materials->getSet().get();

		Vector<ImageMemoryBarrier> outputImageBarriers;
		Vector<BufferMemoryBarrier> outputBufferBarriers;

		doFinalizeTransfer(materials, outputImageBarriers, outputBufferBarriers);

		if (!outputBufferBarriers.empty() && !outputImageBarriers.empty()) {
			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				outputBufferBarriers, outputImageBarriers);
		}

		prepareRenderPass(buf);

		_data->impl.cast<RenderPassImpl>()->perform(*this, buf, [&] {
			writeCommands(materials, buf);
		});

		finalizeRenderPass(buf);
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void Render3dPassHandle::prepareRenderPass(CommandBuffer &buf) {

}

void Render3dPassHandle::writeCommands(gl::MaterialSet * materials, CommandBuffer &buf) {
	auto pass = (RenderPassImpl *)_data->impl.get();
	Buffer *indexBuffer = nullptr;

	struct PushConstantBlock {
		uint32_t materialIdx;
		uint32_t vertexBufferIdx;
	};

	PushConstantBlock pushConstantBlock{
		maxOf<uint32_t>(),
		maxOf<uint32_t>(),
	};

	for (auto &it : _vertexes->getIndexes()) {
		if (it.indexBuffer != indexBuffer) {
			buf.cmdBindIndexBuffer(it.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			indexBuffer = it.indexBuffer;
		}

		auto block = PushConstantBlock{ it.materialIdx, it.vertexBufferIdx };
		if (block != pushConstantBlock) {
			pushConstantBlock = block;
			buf.cmdPushConstants(pass->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0, BytesView((const uint8_t *)&pushConstantBlock, sizeof(PushConstantBlock)));
		}

		buf.cmdDrawIndexed(it.indexCount, it.instanceCount, it.indexOffset, 0, it.instanceOffset);
	}
}

void Render3dPassHandle::finalizeRenderPass(CommandBuffer &buf) {

}

}
