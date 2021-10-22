/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkMaterialRenderPass.h"
#include "XLVkDevice.h"
#include "XLVkFramebuffer.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkPipeline.h"
#include "XLVkBuffer.h"
#include "XLVkFrame.h"
#include "XLVkTextureSet.h"

namespace stappler::xenolith::vk {

MaterialVertexAttachment::~MaterialVertexAttachment() { }

bool MaterialVertexAttachment::init(StringView str, const gl::BufferInfo &info, Vector<Rc<gl::Material>> &&initial) {
	return MaterialAttachment::init(str, info, [] (uint8_t *, const gl::Material *) {
		return false;
	}, sizeof(uint32_t) * 4, move(initial));
}

Rc<gl::AttachmentHandle> MaterialVertexAttachment::makeFrameHandle(const gl::FrameHandle &handle) {
	return Rc<MaterialVertexAttachmentHandle>::create(this, handle);
}

MaterialVertexAttachmentHandle::~MaterialVertexAttachmentHandle() { }

bool MaterialVertexAttachmentHandle::init(const Rc<gl::Attachment> &a, const gl::FrameHandle &handle) {
	if (BufferAttachmentHandle::init(a, handle)) {
		_materials = ((MaterialVertexAttachment *)a.get())->getMaterials();
		return true;
	}
	return false;
}

bool MaterialVertexAttachmentHandle::isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &desc,
		uint32_t, bool isExternal) const {
	return _materials->getGeneration() != ((gl::MaterialAttachmentDescriptor *)desc.descriptor)->getBoundGeneration();
}

bool MaterialVertexAttachmentHandle::writeDescriptor(const RenderPassHandle &handle, const gl::PipelineDescriptor &descriptors,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = ((Buffer *)_materials->getBuffer().get())->getBuffer();
	info.offset = 0;
	info.range = ((Buffer *)_materials->getBuffer().get())->getSize();
	return true;
}

VertexMaterialAttachment::~VertexMaterialAttachment() { }

Rc<gl::AttachmentHandle> VertexMaterialAttachment::makeFrameHandle(const gl::FrameHandle &handle) {
	return Rc<VertexMaterialAttachmentHandle>::create(this, handle);
}

VertexMaterialAttachmentHandle::~VertexMaterialAttachmentHandle() { }

bool VertexMaterialAttachmentHandle::submitInput(gl::FrameHandle &handle, Rc<gl::AttachmentInputData> &&data) {
	if (auto d = data.cast<gl::VertexData>()) {
		handle.performInQueue([this, d = move(d)] (gl::FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [this] (gl::FrameHandle &handle, bool success) {
			if (success) {
				handle.setInputSubmitted(this);
			} else {
				handle.invalidate();
			}
		}, this);
		return true;
	}
	return false;
}

bool VertexMaterialAttachmentHandle::isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	return true;
}

bool VertexMaterialAttachmentHandle::writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = _vertexes->getBuffer();
	info.offset = 0;
	info.range = _vertexes->getSize();
	return true;
}

bool VertexMaterialAttachmentHandle::loadVertexes(gl::FrameHandle &fhandle, const Rc<gl::VertexData> &vertexes) {
	auto handle = dynamic_cast<FrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	_data = vertexes;

	_vertexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U)));
	_vertexes->setData(BytesView((uint8_t *)vertexes->data.data(), vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U)));

	_indexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, vertexes->indexes.size() * sizeof(uint32_t)));
	_indexes->setData(BytesView((uint8_t *)vertexes->indexes.data(), vertexes->indexes.size() * sizeof(uint32_t)));

	return true;
}

bool MaterialRenderPass::init(StringView name, gl::RenderOrdering ord, size_t subpassCount) {
	return RenderPass::init(name, gl::RenderPassType::Graphics, ord, subpassCount);
}

Rc<gl::RenderPassHandle> MaterialRenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<MaterialRenderPassHandle>::create(*this, data, handle);
}

void MaterialRenderPass::prepare(gl::Device &) {
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<MaterialVertexAttachment *>(it->getAttachment())) {
			_materials = a;
		} else if (auto a = dynamic_cast<VertexMaterialAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

void MaterialRenderPassHandle::addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) {
	RenderPassHandle::addRequiredAttachment(a, h);
	if (h->getAttachment() == ((MaterialRenderPass *)_renderPass.get())->getMaterials()) {
		_materialBuffer = (MaterialVertexAttachmentHandle *)h.get();
	} else if (h->getAttachment() == ((MaterialRenderPass *)_renderPass.get())->getVertexes()) {
		_vertexBuffer = (VertexMaterialAttachmentHandle *)h.get();
	}
}

Vector<VkCommandBuffer> MaterialRenderPassHandle::doPrepareCommands(gl::FrameHandle &handle, uint32_t index) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto targetFb = _data->framebuffers[index].cast<Framebuffer>();
	auto currentExtent = targetFb->getExtent();

	auto materials = _materialBuffer->getMaterials().get();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	Vector<VkImageMemoryBarrier> outputImageBarriers;
	Vector<VkBufferMemoryBarrier> outputBufferBarriers;

	doFinalizeTransfer(materials, buf, outputImageBarriers, outputBufferBarriers);

	if (!outputBufferBarriers.empty() && !outputImageBarriers.empty()) {
		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			0, nullptr,
			outputBufferBarriers.size(), outputBufferBarriers.data(),
			outputImageBarriers.size(), outputImageBarriers.data());
	}

	VkRenderPassBeginInfo renderPassInfo { };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _data->impl.cast<RenderPassImpl>()->getRenderPass();
	renderPassInfo.framebuffer = targetFb->getFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;
	table->vkCmdBeginRenderPass(buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	table->vkCmdSetViewport(buf, 0, 1, &viewport);

	VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

	prepareMaterialCommands(materials, handle, buf);

	table->vkCmdEndRenderPass(buf);
	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

void MaterialRenderPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, gl::FrameHandle &handle, VkCommandBuffer &buf) {
	auto table = _device->getTable();

	auto pass = (RenderPassImpl *)_data->impl.get();

	// bind primary descriptors
	// default texture set comes with other sets
	table->vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->getPipelineLayout(),
		0, // first set
		pass->getDescriptorSets().size(), pass->getDescriptorSets().data(), // sets
		0, nullptr // dynamic offsets
	);

	// bind global indexes
	auto idx = _vertexBuffer->getIndexes()->getBuffer();
	table->vkCmdBindIndexBuffer(buf, idx, 0, VK_INDEX_TYPE_UINT32);

	uint32_t boundTextureSetIndex = maxOf<uint32_t>();
	gl::Pipeline *boundPipeline = nullptr;

	for (auto &materialVertexSpan : _vertexBuffer->getVertexData()) {
		auto material = materials->getMaterial(materialVertexSpan.material);
		if (!material) {
			continue;
		}

		auto pipeline = material->getPipeline()->pipeline;
		auto textureSetIndex =  material->getLayoutIndex();

		if (pipeline != boundPipeline) {
			table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline.get())->getPipeline());
			boundPipeline = pipeline;
		}

		if (textureSetIndex != boundTextureSetIndex) {
			if (auto l = materials->getLayout(textureSetIndex)) {
				auto s = (TextureSet *)l->set.get();
				auto set = s->getSet();
				// rebind texture set at last index
				table->vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->getPipelineLayout(),
					1,
					1, &set, // sets
					0, nullptr // dynamic offsets
				);
				boundTextureSetIndex = textureSetIndex;
			} else {
				stappler::log::vtext("MaterialRenderPassHandle", "Invalid textureSetlayout: ", textureSetIndex);
				return;
			}
		}

		table->vkCmdDrawIndexed(buf,
				materialVertexSpan.indexCount, // indexCount
				materialVertexSpan.instanceCount, // instanceCount
				materialVertexSpan.firstIndex, // firstIndex
				0, // int32_t   vertexOffset
				0  // uint32_t  firstInstance
		);
	}
}

void MaterialRenderPassHandle::doFinalizeTransfer(gl::MaterialSet * materials, VkCommandBuffer buf,
		Vector<VkImageMemoryBarrier> &outputImageBarriers, Vector<VkBufferMemoryBarrier> &outputBufferBarriers) {
	auto b = (Buffer *)materials->getBuffer().get();
	if (auto barrier = b->getPendingBarrier()) {
		outputBufferBarriers.emplace_back(*barrier);
		b->dropPendingBarrier();
	}

	for (auto &it : materials->getLayouts()) {
		auto &pending = ((TextureSet *)it.set.get())->getPendingBarriers();
		for (auto &barrier : pending) {
			outputImageBarriers.emplace_back(barrier);
		}
		((TextureSet *)it.set.get())->dropPendingBarriers();
	}
}

}
