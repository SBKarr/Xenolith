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

#include "XLVkMaterialVertexPass.h"
#include "XLVkTextureSet.h"
#include "XLVkAllocator.h"
#include "XLVkBuffer.h"
#include "XLVkPipeline.h"
#include "XLGlCommandList.h"

namespace stappler::xenolith::vk {

MaterialAttachment::~MaterialAttachment() { }

bool MaterialAttachment::init(StringView str, const gl::BufferInfo &info, Vector<Rc<gl::Material>> &&initial) {
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

auto MaterialAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<MaterialAttachmentHandle>::create(this, handle);
}

MaterialAttachmentHandle::~MaterialAttachmentHandle() { }

bool MaterialAttachmentHandle::init(const Rc<Attachment> &a, const FrameQueue &handle) {
	if (BufferAttachmentHandle::init(a, handle)) {
		return true;
	}
	return false;
}

bool MaterialAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &desc,
		uint32_t, bool isExternal) const {
	return _materials && _materials->getGeneration() != ((gl::MaterialAttachmentDescriptor *)desc.descriptor)->getBoundGeneration();
}

bool MaterialAttachmentHandle::writeDescriptor(const QueuePassHandle &handle, DescriptorBufferInfo &info) {
	if (!_materials) {
		return false;
	}

	auto b = _materials->getBuffer();
	if (!b) {
		return false;
	}
	info.buffer = ((Buffer *)b.get());
	info.offset = 0;
	info.range = info.buffer->getSize();
	return true;
}

const MaterialAttachment *MaterialAttachmentHandle::getMaterialAttachment() const {
	return (MaterialAttachment *)_attachment.get();
}

const Rc<gl::MaterialSet> MaterialAttachmentHandle::getSet() const {
	if (!_materials) {
		_materials = getMaterialAttachment()->getMaterials();
	}
	return _materials;
}

VertexMaterialAttachment::~VertexMaterialAttachment() { }

bool VertexMaterialAttachment::init(StringView name, const gl::BufferInfo &info, const MaterialAttachment *m) {
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
		_materials = (const MaterialAttachmentHandle *)materials->handle.get();
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

bool VertexMaterialAttachmentHandle::writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &info) {
	switch (info.index) {
	case 0:
		info.buffer = _vertexes;
		info.offset = 0;
		info.range = _vertexes->getSize();
		return true;
		break;
	case 1:
		info.buffer = _transforms;
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

struct VertexMaterialDrawPlan {
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

	struct WriteTarget {
		uint8_t *transform;
		uint8_t *vertexes;
		uint8_t *indexes;
	};

	Extent2 surfaceExtent;
	gl::SurfaceTransformFlags transform = gl::SurfaceTransformFlags::Identity;

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

	std::forward_list<Vector<gl::TransformedVertexData>> deferredTmp;

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = 0;

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;
	uint32_t transformIdx = 0;

	uint32_t solidCmds = 0;
	uint32_t surfaceCmds = 0;
	uint32_t transparentCmds = 0;

	VertexMaterialDrawPlan(const gl::FrameContraints &constraints)
	: surfaceExtent{constraints.extent}, transform(constraints.transform) { }

	void emplaceWritePlan(const gl::Material *material, std::unordered_map<gl::MaterialId, MaterialWritePlan> &writePlan,
				const gl::Command *c, const gl::CmdGeneral *cmd, SpanView<gl::TransformedVertexData> vertexes) {
		auto it = writePlan.find(cmd->material);
		if (it == writePlan.end()) {
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
	}

	void pushVertexData(gl::MaterialSet *materialSet, const gl::Command *c, const gl::CmdVertexArray *cmd) {
		auto material = materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}
		if (material->getPipeline()->isSolid()) {
			emplaceWritePlan(material, solidWritePlan, c, cmd, cmd->vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(material, surfaceWritePlan, c, cmd, cmd->vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, std::unordered_map<gl::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(material, v->second, c, cmd, cmd->vertexes);
		}
	};

	void pushDeferred(gl::MaterialSet *materialSet, const gl::Command *c, const gl::CmdDeferred *cmd) {
		auto material = materialSet->getMaterialById(cmd->material);
		if (!material) {
			return;
		}

		if (!cmd->deferred->isWaitOnReady()) {
			if (!cmd->deferred->isReady()) {
				return;
			}
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
			emplaceWritePlan(material, solidWritePlan, c, cmd, vertexes);
		} else if (cmd->renderingLevel == RenderingLevel::Surface) {
			emplaceWritePlan(material, surfaceWritePlan, c, cmd, vertexes);
		} else {
			auto v = transparentWritePlan.find(cmd->zPath);
			if (v == transparentWritePlan.end()) {
				v = transparentWritePlan.emplace(cmd->zPath, std::unordered_map<gl::MaterialId, MaterialWritePlan>()).first;
			}
			emplaceWritePlan(material, v->second, c, cmd, vertexes);
		}
	}

	void updatePathsDepth() {
		float depthScale = 1.0f / float(paths.size() + 1);
		float depthOffset = 1.0f - depthScale;
		for (auto &it : paths) {
			it.second = depthOffset;
			depthOffset -= depthScale;
		}
	}

	void pushInitial(WriteTarget &writeTarget) {
		gl::TransformObject val;
		memcpy(writeTarget.transform, &val, sizeof(gl::TransformObject));
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

		switch (transform) {
		case gl::SurfaceTransformFlags::Rotate90:
			vertexes[0].tex = Vec2::UNIT_Y;
			vertexes[1].tex = Vec2::ONE;
			vertexes[2].tex = Vec2::UNIT_X;
			vertexes[3].tex = Vec2::ZERO;
			break;
		case gl::SurfaceTransformFlags::Rotate180:
			vertexes[0].tex = Vec2::ONE;
			vertexes[1].tex = Vec2::UNIT_X;
			vertexes[2].tex = Vec2::ZERO;
			vertexes[3].tex = Vec2::UNIT_Y;
			break;
		case gl::SurfaceTransformFlags::Rotate270:
			vertexes[0].tex = Vec2::UNIT_X;
			vertexes[1].tex = Vec2::ZERO;
			vertexes[2].tex = Vec2::UNIT_Y;
			vertexes[3].tex = Vec2::ONE;
			break;
		default:
			break;
		}

		auto target = (gl::Vertex_V4F_V4F_T2F2U *)writeTarget.vertexes + vertexOffset;
		memcpy(target, (uint8_t *)vertexes.data(), vertexes.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));
		memcpy(writeTarget.indexes, (uint8_t *)indexes.data(), indexes.size() * sizeof(uint32_t));

		vertexOffset += vertexes.size();
		indexOffset += indexes.size();
	}

	uint32_t rotateObject(uint32_t obj, uint32_t idx) {
		auto anchor = (obj >> 16) & 0x3;
		return (obj & ~0x30000) | (((anchor + idx) % 4) << 16);
	}

	Vec2 rotateVec(const Vec2 &vec) {
		switch (transform) {
			case gl::SurfaceTransformFlags::Rotate90:
				return Vec2(-vec.y, vec.x);
				break;
			case gl::SurfaceTransformFlags::Rotate180:
				return Vec2(-vec.x, -vec.y);
				break;
			case gl::SurfaceTransformFlags::Rotate270:
				return Vec2(vec.y, -vec.x);
				break;
			default:
				break;
		}
		return vec;
	}

	void writeRotatedTexture(const gl::ImageAtlas *atlas, const Mat4 &inverseTransform, gl::Vertex_V4F_V4F_T2F2U &t, font::FontAtlasValue *d) {
		Vec2 scaledPos = inverseTransform * (rotateVec(d->pos) / Vec2(surfaceExtent.width, surfaceExtent.height) * 2.0f);
		t.pos.x += scaledPos.x;
		t.pos.y += scaledPos.y;
		t.tex = d->tex;
	}

	void pushVertexes(WriteTarget &writeTarget, const gl::MaterialId &materialId, const MaterialWritePlan &plan,
				const gl::CmdGeneral *cmd, const gl::TransformObject &transform, gl::VertexData *vertexes) {
		auto target = (gl::Vertex_V4F_V4F_T2F2U *)writeTarget.vertexes + vertexOffset;
		memcpy(target, (uint8_t *)vertexes->data.data(),
				vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U));

		memcpy(writeTarget.transform + transtormOffset, &transform, sizeof(gl::TransformObject));

		float atlasScaleX = 1.0f;
		float atlasScaleY = 1.0f;
		Mat4 inverseTransform;

		if (plan.atlas) {
			auto ext = plan.atlas->getImageExtent();
			atlasScaleX = 1.0f / ext.width;
			atlasScaleY = 1.0f / ext.height;
			inverseTransform = transform.transform.getInversed();
		}

		size_t idx = 0;
		for (; idx < vertexes->data.size(); ++ idx) {
			auto &t = target[idx];
			t.material = transformIdx | transformIdx << 16;

			if (plan.atlas && t.object) {
				if (font::FontAtlasValue *d = (font::FontAtlasValue *)plan.atlas->getObjectByName(t.object)) {
					// scale to (-1.0, 1.0), then transform into command space
					writeRotatedTexture(plan.atlas, inverseTransform, t, d);
				} else {
					std::cout << "VertexMaterialDrawPlan: Object not found: " << t.object << " " << string::toUtf8<Interface>(char16_t(t.object)) << "\n";
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

		auto indexTarget = (uint32_t *)writeTarget.indexes + indexOffset;

		for (auto &it : vertexes->indexes) {
			*(indexTarget++) = it + vertexOffset;
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size();
		transtormOffset += sizeof(gl::TransformObject);
		++ transformIdx;

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
	}

	void drawWritePlan(Vector<gl::VertexSpan> &spans, WriteTarget &writeTarget, std::unordered_map<gl::MaterialId, MaterialWritePlan> &writePlan) {
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
						gl::TransformObject val(iit.mat);

						auto pathIt = paths.find(cmd.cmd->zPath);
						if (pathIt != paths.end()) {
							val.offset.z = pathIt->second;
						}

						auto f16 = halffloat::encode(cmd.cmd->depthValue);
						auto value = halffloat::decode(f16);
						val.shadow = Vec4(value, value, value, value);

						pushVertexes(writeTarget, it->first, it->second, cmd.cmd, val, iit.data.get());
					}
				}

				spans.emplace_back(gl::VertexSpan({ it->first, materialIndexes, 1, indexOffset - materialIndexes, state.first}));
			}
		}
	}

	void pushAll(Vector<gl::VertexSpan> &spans, WriteTarget &writeTarget) {
		pushInitial(writeTarget);

		uint32_t counter = 0;
		drawWritePlan(spans, writeTarget, solidWritePlan);

		solidCmds = spans.size() - counter;
		counter = spans.size();

		drawWritePlan(spans, writeTarget, surfaceWritePlan);

		surfaceCmds = spans.size() - counter;
		counter = spans.size();

		for (auto &it : transparentWritePlan) {
			drawWritePlan(spans, writeTarget, it.second);
		}

		transparentCmds = spans.size() - counter;
	}
};

bool VertexMaterialAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<gl::CommandList> &commands) {
	auto handle = dynamic_cast<FrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	VertexMaterialDrawPlan plan(fhandle.getFrameConstraints());

	auto cmd = commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case gl::CommandType::CommandGroup:
			break;
		case gl::CommandType::VertexArray:
			plan.pushVertexData(_materialSet.get(), cmd, (const gl::CmdVertexArray *)cmd->data);
			break;
		case gl::CommandType::Deferred:
			plan.pushDeferred(_materialSet.get(), cmd, (const gl::CmdDeferred *)cmd->data);
			break;
		case gl::CommandType::ShadowArray:
		case gl::CommandType::ShadowDeferred:
			break;
		case gl::CommandType::SdfGroup2D:
			break;
		}
		cmd = cmd->next;
	}

	if (plan.globalWritePlan.vertexes == 0 || plan.globalWritePlan.indexes == 0) {
		return true;
	}

	plan.updatePathsDepth();

	auto devFrame = static_cast<DeviceFrameHandle *>(handle);

	auto &pool = devFrame->getMemPool(this);

	// create buffers
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, (plan.globalWritePlan.indexes + 6) * sizeof(uint32_t)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, (plan.globalWritePlan.vertexes + 4) * sizeof(gl::Vertex_V4F_V4F_T2F2U)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, (plan.globalWritePlan.transforms + 1) * sizeof(gl::TransformObject)));

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

	VertexMaterialDrawPlan::WriteTarget writeTarget{transformMap.ptr, vertexesMap.ptr, indexesMap.ptr};

	// write initial full screen quad
	plan.pushAll(_spans, writeTarget);


	if (fhandle.isPersistentMapping()) {
		_vertexes->unmap(vertexesMap, true);
		_indexes->unmap(indexesMap, true);
		_transforms->unmap(transformMap, true);
	} else {
		_vertexes->setData(vertexData);
		_indexes->setData(indexData);
		_transforms->setData(transformData);
	}

	_drawStat.vertexes = plan.globalWritePlan.vertexes - plan.excludeVertexes;
	_drawStat.triangles = (plan.globalWritePlan.indexes - plan.excludeIndexes) / 3;
	_drawStat.zPaths = plan.paths.size();
	_drawStat.drawCalls = _spans.size();
	_drawStat.materials = _materialSet->getMaterials().size();
	_drawStat.solidCmds = plan.solidCmds;
	_drawStat.surfaceCmds = plan.surfaceCmds;
	_drawStat.transparentCmds = plan.transparentCmds;

	commands->sendStat(_drawStat);

	_commands = commands;
	return true;
}

gl::ImageFormat MaterialVertexPass::selectDepthFormat(SpanView<gl::ImageFormat> formats) {
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

bool MaterialVertexPass::init(StringView name, RenderOrdering ord, size_t subpassCount) {
	return QueuePass::init(name, gl::RenderPassType::Graphics, ord, subpassCount);
}

auto MaterialVertexPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<MaterialVertexPassHandle>::create(*this, handle);
}

void MaterialVertexPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<MaterialAttachment *>(it->getAttachment())) {
			_materials = a;
		} else if (auto a = dynamic_cast<VertexMaterialAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

VkRect2D MaterialVertexPassHandle::rotateScissor(const gl::FrameContraints &constraints, const URect &scissor) {
	VkRect2D scissorRect{
		{ int32_t(scissor.x), int32_t(constraints.extent.height - scissor.y - scissor.height) },
		{ scissor.width, scissor.height }
	};

	switch (constraints.transform) {
	case gl::SurfaceTransformFlags::Rotate90:
		scissorRect.offset.y = scissor.x;
		scissorRect.offset.x = scissor.y;
		std::swap(scissorRect.extent.width, scissorRect.extent.height);
		break;
	case gl::SurfaceTransformFlags::Rotate180:
		scissorRect.offset.y = scissor.y;
		break;
	case gl::SurfaceTransformFlags::Rotate270:
		scissorRect.offset.y = constraints.extent.height - scissor.x - scissor.width;
		scissorRect.offset.x = constraints.extent.width - scissor.y - scissor.height;
		//scissorRect.offset.x = extent.height - scissor.y;
		std::swap(scissorRect.extent.width, scissorRect.extent.height);
		break;
	default: break;
	}

	return scissorRect;
}

bool MaterialVertexPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (MaterialVertexPass *)_renderPass.get();

	if (auto materialBuffer = q.getAttachment(pass->getMaterials())) {
		_materialBuffer = (const MaterialAttachmentHandle *)materialBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const VertexMaterialAttachmentHandle *)vertexBuffer->handle.get();
	}

	_constraints = q.getFrame()->getFrameConstraints();

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<const CommandBuffer *> MaterialVertexPassHandle::doPrepareCommands(FrameHandle &handle) {
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		auto materials = _materialBuffer->getSet().get();

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
			prepareMaterialCommands(materials, buf);
		});

		finalizeRenderPass(buf);
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void MaterialVertexPassHandle::prepareRenderPass(CommandBuffer &) { }

void MaterialVertexPassHandle::prepareMaterialCommands(gl::MaterialSet * materials, CommandBuffer &buf) {
	auto &fb = getFramebuffer();
	auto currentExtent = fb->getExtent();
	auto commands = _vertexBuffer->popCommands();
	auto pass = (RenderPassImpl *)_data->impl.get();

	if (_vertexBuffer->empty() || !_vertexBuffer->getIndexes() || !_vertexBuffer->getVertexes()) {
		return;
	}

	VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	buf.cmdSetViewport(0, makeSpanView(&viewport, 1));

	VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));

	// bind primary descriptors
	// default texture set comes with other sets
	buf.cmdBindDescriptorSets(pass);

	// bind global indexes
	buf.cmdBindIndexBuffer(_vertexBuffer->getIndexes(), 0, VK_INDEX_TYPE_UINT32);

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
					auto scissorRect = rotateScissor(_constraints, state->scissor);
					buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
					dynamicState.scissor = state->scissor;
				}
			} else {
				dynamicState.enabled |= renderqueue::DynamicState::Scissor;
				auto scissorRect = rotateScissor(_constraints, state->scissor);
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
				dynamicState.scissor = state->scissor;
			}
		} else {
			if (dynamicState.isScissorEnabled()) {
				dynamicState.enabled &= ~(renderqueue::DynamicState::Scissor);
				VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
				buf.cmdSetScissor(0, makeSpanView(&scissorRect, 1));
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
			buf.cmdBindPipeline((GraphicPipeline *)pipeline.get());
			boundPipeline = pipeline;
		}

		if (textureSetIndex != boundTextureSetIndex) {
			auto l = materials->getLayout(textureSetIndex);
			if (l && l->set) {
				auto s = (TextureSet *)l->set.get();
				auto set = s->getSet();

				// rebind texture set at last index
				buf.cmdBindDescriptorSets((RenderPassImpl *)_data->impl.get(), makeSpanView(&set, 1), 1);
				boundTextureSetIndex = textureSetIndex;
			} else {
				stappler::log::vtext("MaterialRenderPassHandle", "Invalid textureSetlayout: ", textureSetIndex);
				return;
			}
		}

		enableState(materialVertexSpan.state);

		buf.cmdPushConstants(pass->getPipelineLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, BytesView((const uint8_t *)&materialOrderIdx, sizeof(uint32_t)));

		buf.cmdDrawIndexed(
			materialVertexSpan.indexCount, // indexCount
			materialVertexSpan.instanceCount, // instanceCount
			materialVertexSpan.firstIndex, // firstIndex
			0, // int32_t   vertexOffset
			0  // uint32_t  firstInstance
		);
	}
}

void MaterialVertexPassHandle::finalizeRenderPass(CommandBuffer &) { }

void MaterialVertexPassHandle::doFinalizeTransfer(gl::MaterialSet * materials,
		Vector<ImageMemoryBarrier> &outputImageBarriers, Vector<BufferMemoryBarrier> &outputBufferBarriers) {
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
