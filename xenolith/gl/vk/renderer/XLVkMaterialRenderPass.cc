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

#include "XLGlCommandList.h"
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
	return MaterialAttachment::init(str, info, [] (uint8_t *target, const gl::Material *material) {
		auto &images = material->getImages();
		if (!images.empty()) {
			auto &image = images.front();
			uint32_t sampler = image.sampler;
			memcpy(target, &sampler, sizeof(uint32_t));
			memcpy(target + sizeof(uint32_t), &image.descriptor, sizeof(uint32_t));
			memcpy(target + sizeof(uint32_t) * 2, &image.set, sizeof(uint32_t));
			return true;
		}
		return false;
	}, [] (Rc<gl::TextureSet> &&set) {
		auto s = (TextureSet *)set.get();
		s->getDevice()->getTextureSetLayout()->releaseSet(s);
	}, sizeof(uint32_t) * 4, gl::MaterialType::Basic2D, move(initial));
}

Rc<gl::AttachmentHandle> MaterialVertexAttachment::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<MaterialVertexAttachmentHandle>::create(this, handle);
}

MaterialVertexAttachmentHandle::~MaterialVertexAttachmentHandle() { }

bool MaterialVertexAttachmentHandle::init(const Rc<gl::Attachment> &a, const gl::FrameQueue &handle) {
	if (BufferAttachmentHandle::init(a, handle)) {
		_materials = ((MaterialVertexAttachment *)a.get())->getMaterials();
		return true;
	}
	return false;
}

bool MaterialVertexAttachmentHandle::isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &desc,
		uint32_t, bool isExternal) const {
	return _materials && _materials->getGeneration() != ((gl::MaterialAttachmentDescriptor *)desc.descriptor)->getBoundGeneration();
}

bool MaterialVertexAttachmentHandle::writeDescriptor(const RenderPassHandle &handle, const gl::PipelineDescriptor &descriptors,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	auto b = _materials->getBuffer();
	if (!b) {
		return false;
	}
	info.buffer = ((Buffer *)b.get())->getBuffer();
	info.offset = 0;
	info.range = ((Buffer *)b.get())->getSize();
	return true;
}

VertexMaterialAttachment::~VertexMaterialAttachment() { }

bool VertexMaterialAttachment::init(StringView name, const gl::BufferInfo &info, const MaterialVertexAttachment *m) {
	if (BufferAttachment::init(name, info)) {
		_materials = m;
		return true;
	}
	return false;
}

Rc<gl::AttachmentHandle> VertexMaterialAttachment::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<VertexMaterialAttachmentHandle>::create(this, handle);
}

VertexMaterialAttachmentHandle::~VertexMaterialAttachmentHandle() { }

bool VertexMaterialAttachmentHandle::setup(gl::FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto materials = handle.getAttachment(((VertexMaterialAttachment *)_attachment.get())->getMaterials())) {
		_materials = (const MaterialVertexAttachmentHandle *)materials->handle.get();
	}
	return true;
}

void VertexMaterialAttachmentHandle::submitInput(gl::FrameQueue &handle, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	if (auto d = data.cast<gl::CommandList>()) {
		handle.getFrame().performInQueue([this, d = move(d)] (gl::FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [this, cb = move(cb)] (gl::FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexMaterialAttachmentHandle::submitInput");
	} else {
		cb(false);
	}
}

bool VertexMaterialAttachmentHandle::isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	return _vertexes;
}

bool VertexMaterialAttachmentHandle::writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = _vertexes->getBuffer();
	info.offset = 0;
	info.range = _vertexes->getSize();
	return true;
}

bool VertexMaterialAttachmentHandle::empty() const {
	return !_indexes || !_vertexes;
}

static void checkTriagles(BytesView vertexesData, BytesView indexesData) {
	SpanView<uint32_t> indexes((uint32_t *)indexesData.data(), indexesData.size() / sizeof(uint32_t));
	SpanView<gl::Vertex_V4F_V4F_T2F2U> vertexes(
			(gl::Vertex_V4F_V4F_T2F2U *)vertexesData.data(), vertexesData.size() / sizeof(gl::Vertex_V4F_V4F_T2F2U));

	auto ntriangles = indexes.size() / 3;
	for (size_t i = 0; i < ntriangles; ++ i) {
		uint32_t v1 = indexes[i * 3 + 0];
		uint32_t v2 = indexes[i * 3 + 1];
		uint32_t v3 = indexes[i * 3 + 2];


		if (v1 >= vertexes.size() || v2 >= vertexes.size() || v3 >= vertexes.size()) {
			std::cout << "Invalid input: " << v1 << " " << v2 << " " << v3 << "\n";
		} else {
			auto &vv1 = vertexes[v1];
			auto &vv2 = vertexes[v2];
			auto &vv3 = vertexes[v3];

			//std::cout << v1 << ": " << vv1 << "\n" << v2 << ": " << vv2 << "\n" << v3 << ": " << vv3 << "\n\n";

			/*if (vv1.color != vv2.color || vv1.color != vv3.color || vv2.color != vv3.color) {
				std::cout << "Invalid vertexes: " << vv1 << " " << vv2 << " " << vv3 << "\n";
			}*/
		}
	}
}

bool VertexMaterialAttachmentHandle::loadVertexes(gl::FrameHandle &fhandle, const Rc<gl::CommandList> &commands) {
	auto handle = dynamic_cast<FrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	struct MaterialWritePlan {
		const gl::Material *material = nullptr;
		Rc<gl::ImageAtlas> atlas;
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		std::forward_list<const gl::CmdVertexArray *> commands;
	};

	Map<SpanView<int16_t>, float, ZIndexLess> paths;

	// fill write plan
	MaterialWritePlan globalWritePlan;

	// write plan for objects, that do depth-write and can be drawn out of order
	std::unordered_map<gl::MaterialId, MaterialWritePlan> solidWritePlan;

	// write plan for objects without depth-write, that can be drawn out of order
	std::unordered_map<gl::MaterialId, MaterialWritePlan> surfaceWritePlan;

	// write plan for transparent objects, that should be drawn in order
	Map<SpanView<int16_t>, std::unordered_map<gl::MaterialId, MaterialWritePlan>> transparentWritePlan;

	auto emplaceWritePlan = [&] (std::unordered_map<gl::MaterialId, MaterialWritePlan> &writePlan, const gl::CmdVertexArray *cmd) {
		auto it = writePlan.find(cmd->material);
		if (it == writePlan.end()) {
			auto material = _materials->getMaterials()->getMaterialById(cmd->material);
			if (material) {
				it = writePlan.emplace(cmd->material, MaterialWritePlan()).first;
				it->second.material = material;
				if (auto atlas = it->second.material->getAtlas()) {
					it->second.atlas = atlas;
				}
			}
		}

		if (it != writePlan.end() && it->second.material) {
			for (auto &iit : cmd->vertexes) {
				globalWritePlan.vertexes += iit.second->data.size();
				globalWritePlan.indexes += iit.second->indexes.size();

				it->second.vertexes += iit.second->data.size();
				it->second.indexes += iit.second->indexes.size();
			}
			it->second.commands.emplace_front(cmd);
		}

		auto pathsIt = paths.find(cmd->zPath);
		if (pathsIt == paths.end()) {
			paths.emplace(cmd->zPath, 0.0f);
		}
	};

	auto pushVertexData = [&] (const gl::CmdVertexArray *cmd) {
		auto material = _materials->getMaterials()->getMaterialById(cmd->material);
		if (!material) {
			return;
		}
		if (material->getPipeline()->isSolid()) {
			emplaceWritePlan(solidWritePlan, cmd);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(surfaceWritePlan, cmd);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, std::unordered_map<gl::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(v->second, cmd);
		}
	};

	auto cmd = commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case gl::CommandType::VertexArray:
			pushVertexData((const gl::CmdVertexArray *)cmd->data);
			break;
		case gl::CommandType::CommandGroup:
			break;
		}
		cmd = cmd->next;
	}

	if (globalWritePlan.vertexes == 0 || globalWritePlan.indexes == 0) {
		return true;
	}

	float depthScale = 1.0f / float(paths.size() + 1);
	float depthOffset = 1.0f - depthScale;
	for (auto &it : paths) {
		it.second = depthOffset;
		depthOffset -= depthScale;
	}

	// create buffers
	_indexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, 1024 /*globalWritePlan.indexes*/ * sizeof(uint32_t)));

	_vertexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, 1024 /*globalWritePlan.vertexes*/ * sizeof(gl::Vertex_V4F_V4F_T2F2U)));

	if (!_vertexes || !_indexes) {
		return false;
	}

	DeviceBuffer::MappedRegion vertexesMap, indexesMap;

	Bytes vertexData, indexData;

	if (fhandle.isPersistentMapping()) {
		vertexesMap = _vertexes->map();
		indexesMap = _indexes->map();

		memset(vertexesMap.ptr, 0, sizeof(gl::Vertex_V4F_V4F_T2F2U) * 1024);
		memset(indexesMap.ptr, 0, sizeof(uint32_t) * 1024);
	} else {
		vertexData.resize(globalWritePlan.vertexes * sizeof(gl::Vertex_V4F_V4F_T2F2U));
		indexData.resize(globalWritePlan.indexes * sizeof(uint32_t));

		vertexesMap.ptr = vertexData.data(); vertexesMap.size = vertexData.size();
		indexesMap.ptr = indexData.data(); indexesMap.size = indexData.size();
	}


	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;

	auto pushVertexes = [&] (const gl::MaterialId &materialId, const MaterialWritePlan &plan,
			const gl::CmdVertexArray *cmd, const Mat4 &transform, gl::VertexData *vertexes) {

		auto target = (gl::Vertex_V4F_V4F_T2F2U *)vertexesMap.ptr + vertexOffset;
		memcpy(target, (uint8_t *)vertexes->data.data(),
				vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));

		if (!isGpuTransform()) {
			float depth = 0.0f;
			auto pathIt = paths.find(cmd->zPath);
			if (pathIt != paths.end()) {
				depth = pathIt->second;
			}

			size_t idx = 0;
			for (auto &v : vertexes->data) {
				auto &t = target[idx];
				t.pos = transform * v.pos;
				t.pos.z = depth;
				t.material = materialId;

				if (plan.atlas) {
					t.tex = plan.atlas->getObjectByName(t.object);
				}

				++ idx;
			}

			auto indexTarget = (uint32_t *)indexesMap.ptr + indexOffset;

			idx = 0;
			for (auto &it : vertexes->indexes) {
				indexTarget[idx] = it + vertexOffset;
				++ idx;
			}
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size();

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
	};

	auto pushPadding = [&] {
		auto target = (gl::Vertex_V4F_V4F_T2F2U *)vertexesMap.ptr + vertexOffset;
		target->color = Vec4(1, 0, 0, 1);
		target->tex = Vec2(0, 0); target->material = 0; target->object = 0;
		target->pos = Vec4(-1, -1, 0, 1);

		++ target;
		target->color = Vec4(0, 1, 0, 1);
		target->tex = Vec2(0, 0); target->material = 0; target->object = 0;
		target->pos = Vec4(-1, 1, 0, 1);

		++ target;
		target->color = Vec4(0, 0, 1, 1);
		target->tex = Vec2(0, 0); target->material = 0; target->object = 0;
		target->pos = Vec4(1, 1, 0, 1);

		++ target;
		target->color = Vec4(0, 1, 1, 1);
		target->tex = Vec2(0, 0); target->material = 0; target->object = 0;
		target->pos = Vec4(1, -1, 0, 1);

		auto indexTarget = (uint32_t *)indexesMap.ptr + indexOffset;
		*indexTarget = vertexOffset + 0; ++ indexTarget;
		*indexTarget = vertexOffset + 2; ++ indexTarget;
		*indexTarget = vertexOffset + 1; ++ indexTarget;
		*indexTarget = vertexOffset + 0; ++ indexTarget;
		*indexTarget = vertexOffset + 3; ++ indexTarget;
		*indexTarget = vertexOffset + 2; ++ indexTarget;

		vertexOffset += 4;
		indexOffset += 6;

		materialVertexes += 4;
		materialIndexes += 6;
	};

	auto drawWritePlan = [&] (std::unordered_map<gl::MaterialId, MaterialWritePlan> &writePlan) {
		// optimize draw order, minimize switching pipeline, textureSet and descriptors
		Vector<const Pair<const gl::MaterialId, MaterialWritePlan> *> drawOrder;

		for (auto &it : writePlan) {
			if (drawOrder.empty()) {
				drawOrder.emplace_back(&it);
			} else {
				auto lb = std::lower_bound(drawOrder.begin(), drawOrder.end(), &it,
						[] (const Pair<const gl::MaterialId, MaterialWritePlan> *l, const Pair<const gl::MaterialId, MaterialWritePlan> *r) {
					if (l->second.material->getPipeline() != l->second.material->getPipeline()) {
						return Pipeline::comparePipelineOrdering(*l->second.material->getPipeline(), *r->second.material->getPipeline());
					} else if (l->second.material->getLayoutIndex() != r->second.material->getLayoutIndex()) {
						return l->second.material->getLayoutIndex() < r->second.material->getLayoutIndex();
					} else {
						return l->first < r->first;
					}
				});
				if (lb == drawOrder.end()) {
					drawOrder.emplace_back(&it);
				} else {
					drawOrder.emplace(lb, &it);
				}
			}
		}

		for (auto &it : drawOrder) {
			materialVertexes = 0;
			materialIndexes = 0;

			for (auto &cmd : it->second.commands) {
				for (auto &iit : cmd->vertexes) {
					pushVertexes(it->first, it->second, cmd, iit.first, iit.second.get());
				}
			}

			_spans.emplace_back(gl::VertexSpan({ it->first, materialIndexes, 1, indexOffset - materialIndexes}));

			materialVertexes = 0;
			materialIndexes = 0;

			pushPadding();
		}
	};

	((Device *)fhandle.getDevice())->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		drawWritePlan(solidWritePlan);
		drawWritePlan(surfaceWritePlan);

		for (auto &it : transparentWritePlan) {
			drawWritePlan(it.second);
		}
	});

	/*for (auto &it : paths) {
		for (auto &iit : it.first) {
			std::cout << " " << iit;
		}
		std::cout << "\n";
	}*/

	checkTriagles(BytesView(vertexesMap.ptr, vertexesMap.size), BytesView(indexesMap.ptr, indexesMap.size));

	if (fhandle.isPersistentMapping()) {
		_vertexes->unmap(vertexesMap, true);
		_indexes->unmap(indexesMap, true);
	} else {
		_vertexes->setData(vertexData);
		_indexes->setData(indexData);
	}

	return true;
}

bool MaterialRenderPass::init(StringView name, gl::RenderOrdering ord, size_t subpassCount) {
	return RenderPass::init(name, gl::RenderPassType::Graphics, ord, subpassCount);
}

Rc<gl::RenderPassHandle> MaterialRenderPass::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<MaterialRenderPassHandle>::create(*this, handle);
}

void MaterialRenderPass::prepare(gl::Device &dev) {
	RenderPass::prepare(dev);
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<MaterialVertexAttachment *>(it->getAttachment())) {
			_materials = a;
		} else if (auto a = dynamic_cast<VertexMaterialAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

bool MaterialRenderPassHandle::prepare(gl::FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialRenderPass *)_renderPass.get();

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materialBuffer = (const MaterialVertexAttachmentHandle *)materialBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const VertexMaterialAttachmentHandle *)vertexBuffer->handle.get();
	}

	if (!_vertexBuffer->empty()) {
		return RenderPassHandle::prepare(q, move(cb));
	}

	cb(false);
	return true;
}

Vector<VkCommandBuffer> MaterialRenderPassHandle::doPrepareCommands(gl::FrameHandle &) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto currentExtent = getFramebuffer()->getExtent();

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

	_data->impl.cast<RenderPassImpl>()->perform(*this, buf, [&] {
		VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		table->vkCmdSetViewport(buf, 0, 1, &viewport);

		VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

		prepareMaterialCommands(materials, buf);
	});

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

void MaterialRenderPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, VkCommandBuffer &buf) {
	if (!_vertexBuffer->getIndexes() || !_vertexBuffer->getVertexes()) {
		return;
	}

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
		auto materialOrderIdx = materials->getMaterialOrder(materialVertexSpan.material);
		auto material = materials->getMaterialById(materialVertexSpan.material);
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
			auto l = materials->getLayout(textureSetIndex);
			if (l && l->set) {
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

		table->vkCmdPushConstants(buf, pass->getPipelineLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &materialOrderIdx);

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
	if (!materials) {
		return;
	}

	auto b = (Buffer *)materials->getBuffer().get();
	if (!b) {
		return;
	}

	if (auto barrier = b->getPendingBarrier()) {
		outputBufferBarriers.emplace_back(*barrier);
		b->dropPendingBarrier();
	}

	for (auto &it : materials->getLayouts()) {
		if (it.set) {
			auto &pending = ((TextureSet *)it.set.get())->getPendingBarriers();
			for (auto &barrier : pending) {
				outputImageBarriers.emplace_back(barrier);
			}
			((TextureSet *)it.set.get())->dropPendingBarriers();
		} else {
			log::text("MaterialRenderPassHandle", "No set for material layout");
		}
	}
}

}
