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
#include "XLVkTextureSet.h"

#include "XLDefaultShaders.h"

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

auto MaterialVertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MaterialVertexAttachmentHandle>::create(this, handle);
}

MaterialVertexAttachmentHandle::~MaterialVertexAttachmentHandle() { }

bool MaterialVertexAttachmentHandle::init(const Rc<Attachment> &a, const FrameQueue &handle) {
	if (BufferAttachmentHandle::init(a, handle)) {
		return true;
	}
	return false;
}

bool MaterialVertexAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &desc,
		uint32_t, bool isExternal) const {
	return _materials && _materials->getGeneration() != ((gl::MaterialAttachmentDescriptor *)desc.descriptor)->getBoundGeneration();
}

bool MaterialVertexAttachmentHandle::writeDescriptor(const QueuePassHandle &handle, const PipelineDescriptor &descriptors,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	if (!_materials) {
		return false;
	}

	auto b = _materials->getBuffer();
	if (!b) {
		return false;
	}
	info.buffer = ((Buffer *)b.get())->getBuffer();
	info.offset = 0;
	info.range = ((Buffer *)b.get())->getSize();
	return true;
}

const MaterialVertexAttachment *MaterialVertexAttachmentHandle::getMaterialAttachment() const {
	return (MaterialVertexAttachment *)_attachment.get();
}

const Rc<gl::MaterialSet> MaterialVertexAttachmentHandle::getSet() const {
	if (!_materials) {
		_materials = getMaterialAttachment()->getMaterials();
	}
	return _materials;
}

VertexMaterialAttachment::~VertexMaterialAttachment() { }

bool VertexMaterialAttachment::init(StringView name, const gl::BufferInfo &info, const MaterialVertexAttachment *m) {
	if (BufferAttachment::init(name, info)) {
		_materials = m;
		return true;
	}
	return false;
}

auto VertexMaterialAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<VertexMaterialAttachmentHandle>::create(this, handle);
}

VertexMaterialAttachmentHandle::~VertexMaterialAttachmentHandle() { }

bool VertexMaterialAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	if (auto materials = handle.getAttachment(((VertexMaterialAttachment *)_attachment.get())->getMaterials())) {
		_materials = (const MaterialVertexAttachmentHandle *)materials->handle.get();
	}
	return true;
}

void VertexMaterialAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::CommandList>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		auto &cache = handle.getLoop()->getFrameCache();

		_materialSet = _materials->getSet();
		_drawStat.cachedFramebuffers = cache->getFramebuffersCount();
		_drawStat.cachedImages = cache->getImagesCount();
		_drawStat.cachedImageViews = cache->getImageViewsCount();

		handle.performInQueue([this, d = move(d)] (FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [cb = move(cb)] (FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexMaterialAttachmentHandle::submitInput");
	});
}

bool VertexMaterialAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool isExternal) const {
	switch (idx) {
	case 0:
		return _vertexes;
		break;
	case 1:
		return _transforms;
		break;
	default:
		break;
	}
	return false;
}

bool VertexMaterialAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool, VkDescriptorBufferInfo &info) {
	switch (idx) {
	case 0:
		info.buffer = _vertexes->getBuffer();
		info.offset = 0;
		info.range = _vertexes->getSize();
		return true;
		break;
	case 1:
		info.buffer = _transforms->getBuffer();
		info.offset = 0;
		info.range = _transforms->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}

bool VertexMaterialAttachmentHandle::empty() const {
	return !_indexes || !_vertexes || !_transforms;
}

Rc<gl::CommandList> VertexMaterialAttachmentHandle::popCommands() const {
	auto ret = move(_commands);
	_commands = nullptr;
	return ret;
}

bool VertexMaterialAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<gl::CommandList> &commands) {
	auto handle = dynamic_cast<FrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	struct PlanCommandInfo {
		const gl::CmdGeneral *cmd;
		SpanView<gl::TransformedVertexData> vertexes;
	};

	struct MaterialWritePlan {
		const gl::Material *material = nullptr;
		Rc<gl::ImageAtlas> atlas;
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		uint32_t transforms = 0;
		Map<gl::StateId, std::forward_list<PlanCommandInfo>> states;
	};

	uint32_t excludeVertexes = 0;
	uint32_t excludeIndexes = 0;

	Map<SpanView<int16_t>, float, ZIndexLess> paths;

	// fill write plan
	MaterialWritePlan globalWritePlan;

	// write plan for objects, that do depth-write and can be drawn out of order
	std::unordered_map<gl::MaterialId, MaterialWritePlan> solidWritePlan;

	// write plan for objects without depth-write, that can be drawn out of order
	std::unordered_map<gl::MaterialId, MaterialWritePlan> surfaceWritePlan;

	// write plan for transparent objects, that should be drawn in order
	Map<SpanView<int16_t>, std::unordered_map<gl::MaterialId, MaterialWritePlan>, ZIndexLess> transparentWritePlan;

	auto emplaceWritePlan = [&] (std::unordered_map<gl::MaterialId, MaterialWritePlan> &writePlan,
			const gl::Command *c, const gl::CmdGeneral *cmd, SpanView<gl::TransformedVertexData> vertexes) {
		auto it = writePlan.find(cmd->material);
		if (it == writePlan.end()) {
			auto material = _materialSet->getMaterialById(cmd->material);
			if (material) {
				it = writePlan.emplace(cmd->material, MaterialWritePlan()).first;
				it->second.material = material;
				if (auto atlas = it->second.material->getAtlas()) {
					it->second.atlas = atlas;
				}
			}
		}

		if (it != writePlan.end() && it->second.material) {
			for (auto &iit : vertexes) {
				globalWritePlan.vertexes += iit.data->data.size();
				globalWritePlan.indexes += iit.data->indexes.size();
				++ globalWritePlan.transforms;

				it->second.vertexes += iit.data->data.size();
				it->second.indexes += iit.data->indexes.size();
				++ it->second.transforms;

				if ((c->flags & gl::CommandFlags::DoNotCount) != gl::CommandFlags::None) {
					excludeVertexes = iit.data->data.size();
					excludeIndexes = iit.data->indexes.size();
				}
			}

			auto iit = it->second.states.find(cmd->state);
			if (iit == it->second.states.end()) {
				iit = it->second.states.emplace(cmd->state, std::forward_list<PlanCommandInfo>()).first;
			}

			iit->second.emplace_front(PlanCommandInfo{cmd, vertexes});
		}

		auto pathsIt = paths.find(cmd->zPath);
		if (pathsIt == paths.end()) {
			paths.emplace(cmd->zPath, 0.0f);
		}
	};

	auto pushVertexData = [&] (const gl::Command *c, const gl::CmdVertexArray *cmd) {
		auto material = _materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}
		if (material->getPipeline()->isSolid()) {
			emplaceWritePlan(solidWritePlan, c, cmd, cmd->vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(surfaceWritePlan, c, cmd, cmd->vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, std::unordered_map<gl::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(v->second, c, cmd, cmd->vertexes);
		}
	};

	std::forward_list<Vector<gl::TransformedVertexData>> deferredTmp;

	auto pushDeferred = [&] (const gl::Command *c, const gl::CmdDeferred *cmd) {
		auto material = _materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}

		auto &vertexes = deferredTmp.emplace_front(cmd->deferred->getData().vec<Interface>());
		//auto vertexes = cmd->deferred->getData().pdup(handle->getPool()->getPool());

		// apply transforms;
		if (cmd->normalized) {
			for (auto &it : vertexes) {
				auto modelTransform = cmd->modelTransform * it.mat;

				Mat4 newMV;
				newMV.m[12] = floorf(modelTransform.m[12]);
				newMV.m[13] = floorf(modelTransform.m[13]);
				newMV.m[14] = floorf(modelTransform.m[14]);

				const_cast<gl::TransformedVertexData &>(it).mat = cmd->viewTransform * newMV;
			}
		} else {
			for (auto &it : vertexes) {
				const_cast<gl::TransformedVertexData &>(it).mat = cmd->viewTransform * cmd->modelTransform * it.mat;
			}
		}

		if (cmd->renderingLevel == RenderingLevel::Solid) {
			emplaceWritePlan(solidWritePlan, c, cmd, vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(surfaceWritePlan, c, cmd, vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, std::unordered_map<gl::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(v->second, c, cmd, vertexes);
		}
	};

	auto cmd = commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case gl::CommandType::CommandGroup:
			break;
		case gl::CommandType::VertexArray:
			pushVertexData(cmd, (const gl::CmdVertexArray *)cmd->data);
			break;
		case gl::CommandType::Deferred:
			pushDeferred(cmd, (const gl::CmdDeferred *)cmd->data);
			break;
		case gl::CommandType::ShadowArray:
		case gl::CommandType::ShadowDeferred:
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
		/*std::cout << "Path:";
		for (auto &z : it.first) {
			std::cout << " " << z;
		}
		std::cout << "\n";*/
		it.second = depthOffset;
		depthOffset -= depthScale;
	}

	auto devFrame = static_cast<DeviceFrameHandle *>(handle);

	auto pool = devFrame->getMemPool(this);

	// create buffers
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, (globalWritePlan.indexes + 6) * sizeof(uint32_t)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, (globalWritePlan.vertexes + 4) * sizeof(gl::Vertex_V4F_V4F_T2F2U)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, (globalWritePlan.transforms + 1) * sizeof(gl::TransformObject)));

	if (!_vertexes || !_indexes || !_transforms) {
		return false;
	}

	DeviceBuffer::MappedRegion vertexesMap, indexesMap, transformMap;

	Bytes vertexData, indexData, transformData;

	if (fhandle.isPersistentMapping()) {
		vertexesMap = _vertexes->map();
		indexesMap = _indexes->map();
		transformMap = _transforms->map();

		memset(vertexesMap.ptr, 0, sizeof(gl::Vertex_V4F_V4F_T2F2U) * 1024);
		memset(indexesMap.ptr, 0, sizeof(uint32_t) * 1024);
	} else {
		vertexData.resize(_vertexes->getSize());
		indexData.resize(_indexes->getSize());
		transformData.resize(_transforms->getSize());

		vertexesMap.ptr = vertexData.data(); vertexesMap.size = vertexData.size();
		indexesMap.ptr = indexData.data(); indexesMap.size = indexData.size();
		transformMap.ptr = transformData.data(); transformMap.size = transformData.size();
	}

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = 0;

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;
	uint32_t transformIdx = 0;

	// write initial full screen quad
	do {
		gl::TransformObject val;
		memcpy(transformMap.ptr, &val, sizeof(gl::TransformObject));
		transtormOffset += sizeof(gl::TransformObject); ++ transformIdx;

		Vector<uint32_t> indexes{ 0, 2, 1, 0, 3, 2 };
		Vector<gl::Vertex_V4F_V4F_T2F2U> vertexes {
			gl::Vertex_V4F_V4F_T2F2U{
				Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::ZERO, 0, 0
			},
			gl::Vertex_V4F_V4F_T2F2U{
				Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::UNIT_Y, 0, 0
			},
			gl::Vertex_V4F_V4F_T2F2U{
				Vec4(1.0f, 1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::ONE, 0, 0
			},
			gl::Vertex_V4F_V4F_T2F2U{
				Vec4(1.0f, -1.0f, 0.0f, 1.0f),
				Vec4::ONE, Vec2::UNIT_X, 0, 0
			}
		};

		auto target = (gl::Vertex_V4F_V4F_T2F2U *)vertexesMap.ptr + vertexOffset;
		memcpy(target, (uint8_t *)vertexes.data(), vertexes.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));
		memcpy(indexesMap.ptr, (uint8_t *)indexes.data(), indexes.size() * sizeof(uint32_t));

		vertexOffset += vertexes.size();
		indexOffset += indexes.size();
	} while (0);


	auto pushVertexes = [&] (const gl::MaterialId &materialId, const MaterialWritePlan &plan,
			const gl::CmdGeneral *cmd, const gl::TransformObject &transform, gl::VertexData *vertexes) {

		auto target = (gl::Vertex_V4F_V4F_T2F2U *)vertexesMap.ptr + vertexOffset;
		memcpy(target, (uint8_t *)vertexes->data.data(),
				vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));

		memcpy(transformMap.ptr + transtormOffset, &transform, sizeof(gl::TransformObject));

		float atlasScaleX = 1.0f;
		float atlasScaleY = 1.0f;

		if (plan.atlas) {
			auto ext = plan.atlas->getImageExtent();
			atlasScaleX = 1.0f / ext.width;
			atlasScaleY = 1.0f / ext.height;
		}

		size_t idx = 0;
		for (; idx < vertexes->data.size(); ++ idx) {
			auto &t = target[idx];
			t.material = transformIdx | transformIdx << 16;

			if (plan.atlas && t.object) {
				if (!plan.atlas->getObjectByName(t.tex, t.object)) {
					auto anchor = font::CharLayout::getAnchorForObject(t.object);
					switch (anchor) {
					case font::FontAnchor::BottomLeft:
						t.tex = Vec2(1.0f - atlasScaleX, 0.0f);
						break;
					case font::FontAnchor::TopLeft:
						t.tex = Vec2(1.0f - atlasScaleX, 0.0f + atlasScaleY);
						break;
					case font::FontAnchor::TopRight:
						t.tex = Vec2(1.0f, 0.0f + atlasScaleY);
						break;
					case font::FontAnchor::BottomRight:
						t.tex = Vec2(1.0f, 0.0f);
						break;
					}
				}
			}
		}

		auto indexTarget = (uint32_t *)indexesMap.ptr + indexOffset;

		for (auto &it : vertexes->indexes) {
			*(indexTarget++) = it + vertexOffset;
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size();
		transtormOffset += sizeof(gl::TransformObject);
		++ transformIdx;

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
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
						return GraphicPipeline::comparePipelineOrdering(*l->second.material->getPipeline(), *r->second.material->getPipeline());
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
			// split order on states

			for (auto &state : it->second.states) {
				materialVertexes = 0;
				materialIndexes = 0;

				for (auto &cmd : state.second) {
					for (auto &iit : cmd.vertexes) {
						gl::TransformObject val{iit.mat};

						auto pathIt = paths.find(cmd.cmd->zPath);
						if (pathIt != paths.end()) {
							val.offset.z = pathIt->second;
						}

						pushVertexes(it->first, it->second, cmd.cmd, val, iit.data.get());
					}
				}

				_spans.emplace_back(gl::VertexSpan({ it->first, materialIndexes, 1, indexOffset - materialIndexes, state.first}));
			}
		}
	};

	drawWritePlan(solidWritePlan);
	drawWritePlan(surfaceWritePlan);

	for (auto &it : transparentWritePlan) {
		drawWritePlan(it.second);
	}

	if (fhandle.isPersistentMapping()) {
		_vertexes->unmap(vertexesMap, true);
		_indexes->unmap(indexesMap, true);
		_transforms->unmap(transformMap, true);
	} else {
		_vertexes->setData(vertexData);
		_indexes->setData(indexData);
		_transforms->setData(transformData);
	}

	_drawStat.vertexes = globalWritePlan.vertexes - excludeVertexes;
	_drawStat.triangles = (globalWritePlan.indexes - excludeIndexes) / 3;
	_drawStat.zPaths = paths.size();
	_drawStat.drawCalls = _spans.size();
	_drawStat.materials = _materialSet->getMaterials().size();

	commands->sendStat(_drawStat);

	_commands = commands;
	return true;
}

static gl::ImageFormat MaterialPass_selectDepthFormat(SpanView<gl::ImageFormat> formats) {
	gl::ImageFormat ret = gl::ImageFormat::Undefined;

	uint32_t score = 0;

	auto selectWithScore = [&] (gl::ImageFormat fmt, uint32_t sc) {
		if (score < sc) {
			ret = fmt;
			score = sc;
		}
	};

	for (auto &it : formats) {
		switch (it) {
		case gl::ImageFormat::D16_UNORM: selectWithScore(it, 12); break;
		case gl::ImageFormat::X8_D24_UNORM_PACK32: selectWithScore(it, 7); break;
		case gl::ImageFormat::D32_SFLOAT: selectWithScore(it, 9); break;
		case gl::ImageFormat::S8_UINT: break;
		case gl::ImageFormat::D16_UNORM_S8_UINT: selectWithScore(it, 11); break;
		case gl::ImageFormat::D24_UNORM_S8_UINT: selectWithScore(it, 10); break;
		case gl::ImageFormat::D32_SFLOAT_S8_UINT: selectWithScore(it, 8); break;
		default: break;
		}
	}

	return ret;
}

struct MaterialPass_Data {
	Rc<ShadowLightDataAttachment> lightDataInput;
	Rc<ShadowImageArrayAttachment> lightBufferArray;
};

static void MaterialPass_makeShadowPass(MaterialPass_Data &data, renderqueue::Queue::Builder &builder, Application &app, Extent2 defaultExtent) {
	using namespace renderqueue;

	auto pass = Rc<vk::ShadowPass>::create("ShadowPass", RenderOrdering(0));
	builder.addRenderPass(pass);

	builder.addComputePipeline(pass, ShadowPass::SdfTrianglesComp,
			builder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp));
	builder.addComputePipeline(pass, ShadowPass::SdfImageComp,
			builder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp));

	data.lightDataInput = Rc<ShadowLightDataAttachment>::create("ShadowLightDataAttachment");
	auto vertexInput = Rc<ShadowVertexAttachment>::create("ShadowVertexAttachment");
	auto triangles = Rc<ShadowTrianglesAttachment>::create("ShadowTrianglesAttachment");

	data.lightBufferArray = Rc<ShadowImageArrayAttachment>::create("LightArray", defaultExtent);

	auto sdf = Rc<ShadowImageAttachment>::create("Sdf", defaultExtent);

	builder.addPassInput(pass, 0, data.lightDataInput, AttachmentDependencyInfo());
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, triangles, AttachmentDependencyInfo());

	builder.addPassOutput(pass, 0, data.lightBufferArray, AttachmentDependencyInfo{
		PipelineStage::ComputeShader, AccessType::ShaderWrite,
		PipelineStage::ComputeShader, AccessType::ShaderWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::StorageImage);

	builder.addPassOutput(pass, 0, sdf, AttachmentDependencyInfo{
		PipelineStage::ComputeShader, AccessType::ShaderWrite,
		PipelineStage::ComputeShader, AccessType::ShaderWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::StorageImage);

	// define global input-output
	builder.addInput(data.lightDataInput);
	builder.addInput(vertexInput);

	// requires same input as light data
	builder.addInput(data.lightBufferArray);
}

static void MaterialPass_makePresentPass(MaterialPass_Data &data, renderqueue::Queue::Builder &builder, Application &app, Extent2 defaultExtent) {
	using namespace renderqueue;

	// load shaders by ref - do not copy data into engine
	auto materialFrag = builder.addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = builder.addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialPass>::create("MaterialSwapchainPass", RenderOrderingHighest);
	builder.addRenderPass(pass);

	auto shaderSpecInfo = Vector<SpecializationInfo>({
		// no specialization required for vertex shader
		materialVert,
		// specialization for fragment shader - use platform-dependent array sizes
		SpecializationInfo(materialFrag, Vector<PredefinedConstant>{
			PredefinedConstant::SamplersArraySize,
			PredefinedConstant::TexturesArraySize
		})
	});

	// pipelines for material-besed rendering
	auto materialPipeline = builder.addGraphicPipeline(pass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = builder.addGraphicPipeline(pass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less)
	}));
	auto surfacePipeline = builder.addGraphicPipeline(pass, 0, "Surface", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::LessOrEqual)
	));

	// pipeline for debugging - draw lines instead of triangles
	builder.addGraphicPipeline(pass, 0, "DebugTriangles", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less),
		LineWidth(1.0f)
	));

	// depth buffer - temporary/transient
	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth",
		gl::ImageInfo(
			defaultExtent,
			gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			MaterialPass_selectDepthFormat(app.getGlLoop()->getSupportedDepthStencilFormat())),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F::WHITE,
			.frameSizeCallback = [] (const FrameQueue &frame) {
				return Extent3(frame.getExtent());
			}
	});

	// swapchain output
	auto out = Rc<vk::ImageAttachment>::create("Output",
		gl::ImageInfo(
			defaultExtent,
			gl::ForceImageUsage(gl::ImageUsage::ColorAttachment),
			platform::graphic::getCommonFormat()),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::PresentSrc,
			.clearOnLoad = true,
			.clearColor = Color4F(1.0f, 1.0f, 1.0f, 1.0f), // Color4F::BLACK;
			.frameSizeCallback = [] (const FrameQueue &frame) {
				return Extent3(frame.getExtent());
			}
	});

	// Material input attachment - per-scene list of materials
	auto &cache = app.getResourceCache();
	auto materialInput = Rc<vk::MaterialVertexAttachment>::create("MaterialInput",
		gl::BufferInfo(gl::BufferUsage::StorageBuffer),

		// ... with predefined list of materials
		Vector<Rc<gl::Material>>({
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getEmptyImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, materialPipeline, cache->getSolidImage(), ColorMode::IntensityChannel),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, transparentPipeline, cache->getSolidImage(), ColorMode()),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, surfacePipeline, cache->getEmptyImage(), ColorMode()),
			Rc<gl::Material>::create(gl::Material::MaterialIdInitial, surfacePipeline, cache->getSolidImage(), ColorMode())
		})
	);

	// Vertex input attachment - per-frame vertex list
	auto vertexInput = Rc<vk::VertexMaterialAttachment>::create("VertexInput",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer), materialInput);

	// define pass input-output
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	builder.addPassInput(pass, 0, materialInput, AttachmentDependencyInfo()); // 1
	if (data.lightDataInput) {
		builder.addPassInput(pass, 0, data.lightDataInput, AttachmentDependencyInfo()); // 2
	}
	if (data.lightBufferArray) {
		auto shadowVert = builder.addProgramByRef("ShadowMergeVert", xenolith::shaders::ShadowMergeVert);
		auto shadowFrag = builder.addProgramByRef("ShadowMergeFrag", xenolith::shaders::ShadowMergeFrag);

		builder.addGraphicPipeline(pass, 0, MaterialPass::ShadowPipeline, Vector<SpecializationInfo>({
			// no specialization required for vertex shader
			shadowVert,
			// specialization for fragment shader - use platform-dependent array sizes
			SpecializationInfo(shadowFrag, Vector<PredefinedConstant>{
				PredefinedConstant::SamplersArraySize,
			})
		}), PipelineMaterialInfo({
			BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
					gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
			DepthInfo()
		}));

		builder.addPassInput(pass, 0, data.lightBufferArray, AttachmentDependencyInfo{ // 3
			PipelineStage::FragmentShader, AccessType::ShaderRead,
			PipelineStage::FragmentShader, AccessType::ShaderRead,

			// can be reused after RenderPass is submitted
			FrameRenderPassState::Submitted,
		}, DescriptorType::SampledImage);
	}

	builder.addPassDepthStencil(pass, 0, depth, AttachmentDependencyInfo{
		PipelineStage::EarlyFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		PipelineStage::LateFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	});
	builder.addPassOutput(pass, 0, out, AttachmentDependencyInfo{
		// first used as color attachment to output colors
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// last used the same way (the only usage for this attachment)
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	});

	builder.addInput(vertexInput);
	builder.addOutput(out);
}

bool MaterialPass::makeDefaultRenderQueue(RenderQueueInfo &info) {
	MaterialPass_Data data;

	if (info.withShadows) {
		MaterialPass_makeShadowPass(data, *info.builder, *info.app, info.extent);
	}
	MaterialPass_makePresentPass(data, *info.builder, *info.app, info.extent);

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
	}

	info.builder->setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	return true;
}

bool MaterialPass::init(StringView name, RenderOrdering ord, size_t subpassCount) {
	return QueuePass::init(name, gl::RenderPassType::Graphics, ord, subpassCount);
}

auto MaterialPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialPassHandle>::create(*this, handle);
}

void MaterialPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<MaterialVertexAttachment *>(it->getAttachment())) {
			_materials = a;
		} else if (auto a = dynamic_cast<VertexMaterialAttachment *>(it->getAttachment())) {
			_vertexes = a;
		} else if (auto a = dynamic_cast<ShadowLightDataAttachment *>(it->getAttachment())) {
			_lights = a;
		} else if (auto a = dynamic_cast<ShadowImageArrayAttachment *>(it->getAttachment())) {
			_array = a;
		}
	}
}

bool MaterialPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialPass *)_renderPass.get();

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materialBuffer = (const MaterialVertexAttachmentHandle *)materialBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const VertexMaterialAttachmentHandle *)vertexBuffer->handle.get();
	}

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = (const ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto arrayImage = q.getAttachment(pass->getArray())) {
		_arrayImage = (const ShadowImageArrayAttachmentHandle *)arrayImage->handle.get();
	}

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<VkCommandBuffer> MaterialPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto materials = _materialBuffer->getSet().get();

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

	auto cIdx = _device->getQueueFamily(QueueOperations::Compute)->index;
	if (cIdx != _pool->getFamilyIdx()) {
		VkBufferMemoryBarrier transferBufferBarrier({
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			cIdx, _pool->getFamilyIdx(),
			_lightsBuffer->getBuffer()->getBuffer(), 0, VK_WHOLE_SIZE,
		});

		auto image = (Image *)_arrayImage->getImage()->getImage().get();

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				1, &transferBufferBarrier,
				1, image->getPendingBarrier());
	} else {
		// no need for buffer barrier, only switch image layout

		auto image = (Image *)_arrayImage->getImage()->getImage().get();
		VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
		VkImageMemoryBarrier transferImageBarrier({
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			image->getImage(), range
		});
		image->setPendingBarrier(transferImageBarrier);

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &transferImageBarrier);
	}

	_data->impl.cast<RenderPassImpl>()->perform(*this, buf, [&] {
		prepareMaterialCommands(materials, buf);
	});

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

void MaterialPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, VkCommandBuffer &buf) {
	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();
	auto table = _device->getTable();
	auto commands = _vertexBuffer->popCommands();

	VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	table->vkCmdSetViewport(buf, 0, 1, &viewport);

	VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

	if (_vertexBuffer->empty() || !_vertexBuffer->getIndexes() || !_vertexBuffer->getVertexes()) {
		return;
	}

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
	gl::GraphicPipeline *boundPipeline = nullptr;

	uint32_t dynamicStateId = 0;
	gl::DrawStateValues dynamicState;

	auto enableState = [&] (uint32_t stateId) {
		if (stateId == dynamicStateId) {
			return;
		}

		auto state = commands->getState(stateId);
		if (!state) {
			return;
		}

		if (state->isScissorEnabled()) {
			if (dynamicState.isScissorEnabled()) {
				if (dynamicState.scissor != state->scissor) {
					VkRect2D scissorRect{ { int32_t(state->scissor.x), int32_t(state->scissor.y) },
						{ state->scissor.width, state->scissor.height } };
					table->vkCmdSetScissor(buf, 0, 1, &scissorRect);
					dynamicState.scissor = state->scissor;
				}
			} else {
				dynamicState.enabled |= renderqueue::DynamicState::Scissor;
				VkRect2D scissorRect{ { int32_t(state->scissor.x), int32_t(state->scissor.y) },
					{ state->scissor.width, state->scissor.height } };
				table->vkCmdSetScissor(buf, 0, 1, &scissorRect);
				dynamicState.scissor = state->scissor;
			}
		} else {
			if (dynamicState.isScissorEnabled()) {
				dynamicState.enabled &= ~(renderqueue::DynamicState::Scissor);
				VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
				table->vkCmdSetScissor(buf, 0, 1, &scissorRect);
			}
		}

		dynamicStateId = stateId;
	};

	for (auto &materialVertexSpan : _vertexBuffer->getVertexData()) {
		auto materialOrderIdx = materials->getMaterialOrder(materialVertexSpan.material);
		auto material = materials->getMaterialById(materialVertexSpan.material);
		if (!material) {
			continue;
		}

		auto pipeline = material->getPipeline()->pipeline;
		auto textureSetIndex =  material->getLayoutIndex();

		if (pipeline != boundPipeline) {
			table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((GraphicPipeline *)pipeline.get())->getPipeline());
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

		enableState(materialVertexSpan.state);

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

	auto pipeline = (GraphicPipeline *)_data->subpasses[0].graphicPipelines.get(StringView(MaterialPass::ShadowPipeline))->pipeline.get();

	viewport = VkViewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	table->vkCmdSetViewport(buf, 0, 1, &viewport);

	scissorRect = VkRect2D{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

	uint32_t samplerIndex = 1;
	table->vkCmdPushConstants(buf, pass->getPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &samplerIndex);

	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

	table->vkCmdDrawIndexed(buf,
		6, // indexCount
		1, // instanceCount
		0, // firstIndex
		0, // int32_t   vertexOffset
		0  // uint32_t  firstInstance
	);
}

void MaterialPassHandle::doFinalizeTransfer(gl::MaterialSet * materials, VkCommandBuffer buf,
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
