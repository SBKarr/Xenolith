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
		uint32_t, bool isExternal) const {
	return _vertexes;
}

bool VertexMaterialAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = _vertexes->getBuffer();
	info.offset = 0;
	info.range = _vertexes->getSize();
	return true;
}

bool VertexMaterialAttachmentHandle::empty() const {
	return !_indexes || !_vertexes;
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
		Map<gl::StateId, std::forward_list<PlanCommandInfo>> states;
	};

	uint32_t excludeVertexes = 0;
	uint32_t excludeIndexes = 0;

	Map<SpanView<int16_t>, float, ZIndexLess> paths;
	std::forward_list<Vector<gl::TransformedVertexData>> deferredResults;

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

				it->second.vertexes += iit.data->data.size();
				it->second.indexes += iit.data->indexes.size();

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

	auto pushDeferred = [&] (const gl::Command *c, const gl::CmdDeferred *cmd) {
		auto material = _materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}

		auto &vertexes = deferredResults.emplace_front(cmd->deferred->getData().vec<Interface>());

		// apply transforms;
		if (cmd->normalized) {
			for (auto &it : vertexes) {
				auto modelTransform = cmd->modelTransform * it.mat;

				Mat4 newMV;
				newMV.m[12] = floorf(modelTransform.m[12]);
				newMV.m[13] = floorf(modelTransform.m[13]);
				newMV.m[14] = floorf(modelTransform.m[14]);

				it.mat = cmd->viewTransform * newMV;
			}
		} else {
			for (auto &it : vertexes) {
				it.mat = cmd->viewTransform * cmd->modelTransform * it.mat;
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

	// create buffers
	_indexes = devFrame->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, globalWritePlan.indexes * sizeof(uint32_t)));

	_vertexes = devFrame->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, globalWritePlan.vertexes * sizeof(gl::Vertex_V4F_V4F_T2F2U)));

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
			const gl::CmdGeneral *cmd, const Mat4 &transform, gl::VertexData *vertexes) {

		auto target = (gl::Vertex_V4F_V4F_T2F2U *)vertexesMap.ptr + vertexOffset;
		memcpy(target, (uint8_t *)vertexes->data.data(),
				vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));

		if (!isGpuTransform()) {
			float depth = 0.0f;
			auto pathIt = paths.find(cmd->zPath);
			if (pathIt != paths.end()) {
				depth = pathIt->second;
			}

			float atlasScaleX = 1.0f;
			float atlasScaleY = 1.0f;

			if (plan.atlas) {
				auto ext = plan.atlas->getImageExtent();
				atlasScaleX = 1.0f / ext.width;
				atlasScaleY = 1.0f / ext.height;
			}

			size_t idx = 0;
			for (auto &v : vertexes->data) {
				auto &t = target[idx];
				t.pos = transform * v.pos;
				t.pos.z = depth;
				t.material = materialId;

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
			// split order on states

			for (auto &state : it->second.states) {
				materialVertexes = 0;
				materialIndexes = 0;

				for (auto &cmd : state.second) {
					for (auto &iit : cmd.vertexes) {
						pushVertexes(it->first, it->second, cmd.cmd, iit.mat, iit.data.get());
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
	} else {
		_vertexes->setData(vertexData);
		_indexes->setData(indexData);
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

bool MaterialPass::makeDefaultRenderQueue(RenderQueueInfo &info) {
	auto selectDepthFormat = [] (SpanView<gl::ImageFormat> formats) {
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
	};

	using namespace renderqueue;

	auto &cache = info.app->getResourceCache();

	// load shaders by ref - do not copy content into engine
	auto materialFrag = info.builder->addProgramByRef("Loader_MaterialVert", xenolith::shaders::MaterialVert);
	auto materialVert = info.builder->addProgramByRef("Loader_MaterialFrag", xenolith::shaders::MaterialFrag);

	// render-to-swapchain RenderPass
	auto pass = Rc<vk::MaterialPass>::create("MaterialSwapchainPass", RenderOrderingHighest);
	info.builder->addRenderPass(pass);

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
	auto materialPipeline = info.builder->addPipeline(pass, 0, "Solid", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(),
		DepthInfo(true, true, gl::CompareOp::Less)
	}));
	auto transparentPipeline = info.builder->addPipeline(pass, 0, "Transparent", shaderSpecInfo, PipelineMaterialInfo({
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less)
	}));
	auto surfacePipeline = info.builder->addPipeline(pass, 0, "Surface", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::LessOrEqual)
	));

	info.builder->addPipeline(pass, 0, "DebugTriangles", shaderSpecInfo, PipelineMaterialInfo(
		BlendInfo(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha, gl::BlendOp::Add,
				gl::BlendFactor::Zero, gl::BlendFactor::One, gl::BlendOp::Add),
		DepthInfo(false, true, gl::CompareOp::Less),
		LineWidth(1.0f)
	));

	// define internal resources (images and buffers)
	gl::Resource::Builder resourceBuilder("LoaderResources");
	if (info.resourceCallback) {
		info.resourceCallback(resourceBuilder);
	}

	info.builder->setInternalResource(Rc<gl::Resource>::create(move(resourceBuilder)));

	ImageAttachment::AttachmentInfo depthAttachmentInfo;
	depthAttachmentInfo.initialLayout = AttachmentLayout::Undefined;
	depthAttachmentInfo.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal;
	depthAttachmentInfo.clearOnLoad = true;
	depthAttachmentInfo.clearColor = Color4F::WHITE;
	depthAttachmentInfo.frameSizeCallback = [] (const FrameQueue &frame) {
		return Extent3(frame.getExtent());
	};

	auto depth = Rc<vk::ImageAttachment>::create("CommonDepth",
		gl::ImageInfo(
			info.extent,
			gl::ForceImageUsage(gl::ImageUsage::DepthStencilAttachment),
			selectDepthFormat(info.app->getGlLoop()->getSupportedDepthStencilFormat())),
		ImageAttachment::AttachmentInfo{
			.initialLayout = AttachmentLayout::Undefined,
			.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
			.clearOnLoad = true,
			.clearColor = Color4F::WHITE,
			.frameSizeCallback = [] (const FrameQueue &frame) {
				return Extent3(frame.getExtent());
			}
	});

	auto out = Rc<vk::ImageAttachment>::create("Output",
		gl::ImageInfo(
			info.extent,
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
	vertexInput->setInputCallback(move(info.vertexInputCallback));

	// define pass input-output
	info.builder->addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo()); // 0
	info.builder->addPassInput(pass, 0, materialInput, AttachmentDependencyInfo()); // 1
	info.builder->addPassDepthStencil(pass, 0, depth, AttachmentDependencyInfo{
		PipelineStage::EarlyFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
		PipelineStage::LateFragmentTest,
			AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	});
	info.builder->addPassOutput(pass, 0, out, AttachmentDependencyInfo{
		// first used as color attachment to output colors
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// last used the same way (the only usage for this attachment)
		PipelineStage::ColorAttachmentOutput, AccessType::ColorAttachmentWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	});

	// define global input-output
	// materialInput is persistent between frames, only vertexes should be provided before rendering started
	info.builder->addInput(vertexInput);
	info.builder->addOutput(out);

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

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<VkCommandBuffer> MaterialPassHandle::doPrepareCommands(FrameHandle &) {
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
	gl::Pipeline *boundPipeline = nullptr;

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
