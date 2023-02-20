/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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
#include "XLVkShadowRenderPass.h"
#include "XLVkBuffer.h"
#include "XLVkRenderPassImpl.h"
#include "XLDefaultShaders.h"
#include "XLGlSdf.h"

namespace stappler::xenolith::vk {

ShadowLightDataAttachmentHandle::~ShadowLightDataAttachmentHandle() { }

void ShadowLightDataAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::ShadowLightInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		auto devFrame = (DeviceFrameHandle *)(&handle);

		_input = move(d);
		_data = devFrame->getMemPool(devFrame)->spawn(AllocationUsage::DeviceLocalHostVisible,
					gl::BufferInfo(((BufferAttachment *)_attachment.get())->getInfo(), sizeof(ShadowLightDataAttachment::LightData)));
		cb(true);
	});
}

bool ShadowLightDataAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	if (_data) {
		return true;
	}
	return false;
}

bool ShadowLightDataAttachmentHandle::writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &info) {
	switch (info.index) {
	case 0:
		info.buffer = _data;
		info.offset = 0;
		info.range = _data->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}

void ShadowLightDataAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, uint32_t trianglesCount, float value, uint32_t gridSize, Extent2) {
	ShadowLightDataAttachment::LightData *data = nullptr;
	DeviceBuffer::MappedRegion mapped;
	if (devFrame->isPersistentMapping()) {
		mapped = _data->map();
		data = (ShadowLightDataAttachment::LightData *)mapped.ptr;
	} else {
		data = new ShadowLightDataAttachment::LightData;
	}

	if (isnan(_input->luminosity)) {
		float l = _input->globalColor.a;
		for (uint32_t i = 0; i < _input->ambientLightCount; ++ i) {
			l += _input->ambientLights[i].color.a;
		}
		for (uint32_t i = 0; i < _input->directLightCount; ++ i) {
			l += _input->directLights[i].color.a;
		}
		_shadowData.luminosity = data->luminosity = 1.0f / l;
	} else {
		_shadowData.luminosity = data->luminosity = 1.0f / _input->luminosity;
	}

	float fullDensity = _input->sceneDensity;
	float shadowDensity = _input->sceneDensity / _input->shadowDensity;
	auto screenSize = devFrame->getFrameConstraints().getScreenSize();

	Extent2 scaledExtent(ceilf(screenSize.width / fullDensity), ceilf(screenSize.height / fullDensity));

	Extent2 shadowExtent(ceilf(screenSize.width / shadowDensity), ceilf(screenSize.height / shadowDensity));
	Vec2 shadowOffset(shadowExtent.width - screenSize.width / shadowDensity, shadowExtent.height - screenSize.height / shadowDensity);

	_shadowData.globalColor = data->globalColor = _input->globalColor * data->luminosity;

	// pre-calculated color with no shadows
	Color4F discardColor = _input->globalColor;
	for (uint i = 0; i < _input->ambientLightCount; ++ i) {
		discardColor = discardColor + (_input->ambientLights[i].color * _input->ambientLights[i].color.a) * data->luminosity;
	}
	discardColor.a = 1.0f;
	_shadowData.discardColor = data->discardColor = discardColor;

	_shadowData.trianglesCount = data->trianglesCount = trianglesCount;
	_shadowData.gridSize = data->gridSize = ceilf(gridSize / fullDensity);
	_shadowData.gridWidth = data->gridWidth = (scaledExtent.width - 1) / data->gridSize + 1;
	_shadowData.gridHeight = data->gridHeight = (scaledExtent.height - 1) / data->gridSize + 1;
	_shadowData.ambientLightCount = data->ambientLightCount = _input->ambientLightCount;
	_shadowData.directLightCount = data->directLightCount = _input->directLightCount;
	_shadowData.bbOffset = data->bbOffset = getBoxOffset(value);
	_shadowData.density = data->density = _input->sceneDensity;
	_shadowData.shadowSdfDensity = data->shadowSdfDensity = 1.0f / _input->shadowDensity;
	_shadowData.shadowDensity = data->shadowDensity = 1.0f / _input->sceneDensity;
	_shadowData.shadowOffset = data->shadowOffset = shadowOffset;
	_shadowData.pix = data->pix = Vec2(1.0f / float(screenSize.width), 1.0f / float(screenSize.height));

	memcpy(data->ambientLights, _input->ambientLights, sizeof(gl::AmbientLightData) * config::MaxAmbientLights);
	memcpy(data->directLights, _input->directLights, sizeof(gl::DirectLightData) * config::MaxDirectLights);
	memcpy(_shadowData.ambientLights, _input->ambientLights, sizeof(gl::AmbientLightData) * config::MaxAmbientLights);
	memcpy(_shadowData.directLights, _input->directLights, sizeof(gl::DirectLightData) * config::MaxDirectLights);

	if (devFrame->isPersistentMapping()) {
		_data->unmap(mapped, true);
	} else {
		_data->setData(BytesView((const uint8_t *)data, sizeof(ShadowLightDataAttachment::LightData)));
		delete data;
	}
}

float ShadowLightDataAttachmentHandle::getBoxOffset(float value) const {
	value = std::max(value, 2.0f);
	float bbox = 0.0f;
	for (size_t i = 0; i < _input->ambientLightCount; ++ i) {
		auto &l = _input->ambientLights[i];
		float n_2 = l.normal.x * l.normal.x + l.normal.y * l.normal.y;
		float m = std::sqrt(n_2) / std::sqrt(1 - n_2);

		bbox = std::max((m * value * 2.0f) + (std::ceil(l.normal.w * value)), bbox);
	}
	return bbox;
}

uint32_t ShadowLightDataAttachmentHandle::getLightsCount() const {
	return _input->ambientLightCount + _input->directLightCount;
}

uint32_t ShadowLightDataAttachmentHandle::getObjectsCount() const {
	return _shadowData.trianglesCount + _shadowData.circlesCount + _shadowData.rectsCount
			+ _shadowData.roundedRectsCount + _shadowData.polygonCount;
}

ShadowVertexAttachmentHandle::~ShadowVertexAttachmentHandle() { }

void ShadowVertexAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
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

		handle.performInQueue([this, d = move(d)] (FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [cb = move(cb)] (FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexMaterialAttachmentHandle::submitInput");
	});
}

bool ShadowVertexAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool isExternal) const {
	switch (idx) {
	case 0: return _indexes; break;
	case 1: return _vertexes; break;
	case 2: return _transforms; break;
	case 3: return _circles; break;
	default: break;
	}
	return false;
}

bool ShadowVertexAttachmentHandle::writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &info) {
	switch (info.index) {
	case 0:
		info.buffer = _indexes;
		info.offset = 0;
		info.range = _indexes->getSize();
		return true;
		break;
	case 1:
		info.buffer = _vertexes;
		info.offset = 0;
		info.range = _vertexes->getSize();
		return true;
		break;
	case 2:
		info.buffer = _transforms;
		info.offset = 0;
		info.range = _transforms->getSize();
		return true;
		break;
	case 3:
		info.buffer = _circles;
		info.offset = 0;
		info.range = _circles->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}

bool ShadowVertexAttachmentHandle::empty() const {
	return !_indexes || !_vertexes || !_transforms;
}

struct ShadowDrawPlan {
	struct PlanCommandInfo {
		const gl::CmdShadow *cmd;
		SpanView<gl::TransformedVertexData> vertexes;
	};

	ShadowDrawPlan(renderqueue::FrameHandle &fhandle) : pool(fhandle.getPool()->getPool()) {

	}

	void emplaceWritePlan(const gl::Command *c, const gl::CmdShadow *cmd, SpanView<gl::TransformedVertexData> v) {
		for (auto &iit : v) {
			vertexes += iit.data->data.size();
			indexes += iit.data->indexes.size();
			++ transforms;
		}

		commands.emplace_front(PlanCommandInfo{cmd, v});
	}

	void pushDeferred(const gl::Command *c, const gl::CmdShadowDeferred *cmd) {
		if (!cmd->deferred->isWaitOnReady()) {
			if (!cmd->deferred->isReady()) {
				return;
			}
		}

		auto vertexes = cmd->deferred->getData().pdup(pool);

		// apply transforms;
		if (cmd->normalized) {
			for (auto &it : vertexes) {
				auto modelTransform = cmd->modelTransform * it.mat;

				Mat4 newMV;
				newMV.m[12] = floorf(modelTransform.m[12]);
				newMV.m[13] = floorf(modelTransform.m[13]);
				newMV.m[14] = floorf(modelTransform.m[14]);

				const_cast<gl::TransformedVertexData &>(it).mat = newMV;
			}
		} else {
			for (auto &it : vertexes) {
				const_cast<gl::TransformedVertexData &>(it).mat = cmd->modelTransform * it.mat;
			}
		}

		emplaceWritePlan(c, cmd, vertexes);
	}

	void pushSdf(const gl::Command *c, const gl::CmdSdfGroup2D *cmd) {
		uint32_t objects = 0;
		for (auto &it : cmd->data) {
			switch (it.type) {
			case gl::SdfShape::Circle2D: ++ circles; ++ objects; break;
			case gl::SdfShape::Triangle2D:
				break;
			default: break;
			}
		}
		if (objects > 0) {
			sdfCommands.emplace_front(cmd);
		}
	}

	memory::pool_t *pool = nullptr;
	uint32_t vertexes = 0;
	uint32_t indexes = 0;
	uint32_t transforms = 0;
	uint32_t circles = 0;
	std::forward_list<PlanCommandInfo> commands;
	std::forward_list<const gl::CmdSdfGroup2D *> sdfCommands;
};

struct ShadowBufferMap {
	DeviceBuffer::MappedRegion region;
	Bytes external;
	DeviceBuffer *buffer;
	bool isPersistent = false;

	~ShadowBufferMap() {
		if (isPersistent) {
			buffer->unmap(region, true);
		} else {
			buffer->setData(external);
		}
	}

	ShadowBufferMap(DeviceBuffer *b, bool persistent) : buffer(b), isPersistent(persistent) {
		if (isPersistent) {
			region = buffer->map();
		} else {
			external.resize(buffer->getSize());
			region.ptr = external.data(); region.size = external.size();
		}
	}
};

bool ShadowVertexAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<gl::CommandList> &commands) {
	auto handle = dynamic_cast<DeviceFrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	// fill write plan

	ShadowDrawPlan plan(fhandle);

	auto cmd = commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case gl::CommandType::CommandGroup:
		case gl::CommandType::VertexArray:
		case gl::CommandType::Deferred:
			break;
		case gl::CommandType::ShadowArray:
			plan.emplaceWritePlan(cmd, (const gl::CmdShadowArray *)cmd->data, ((const gl::CmdShadowArray *)cmd->data)->vertexes);
			break;
		case gl::CommandType::ShadowDeferred:
			plan.pushDeferred(cmd, (const gl::CmdShadowDeferred *)cmd->data);
			break;
		case gl::CommandType::SdfGroup2D:
			plan.pushSdf(cmd, (const gl::CmdSdfGroup2D *)cmd->data);
			break;
		}
		cmd = cmd->next;
	}

	if (plan.vertexes == 0 && plan.indexes == 0 && plan.circles == 0) {
		return true;
	}

	auto &info = ((BufferAttachment *)_attachment.get())->getInfo();

	// create buffers
	auto &pool = handle->getMemPool(handle);
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, std::max((plan.indexes / 3), uint32_t(1)) * sizeof(gl::Triangle2DIndex)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, std::max(plan.vertexes, uint32_t(1)) * sizeof(Vec4)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, std::max((plan.transforms + 1), uint32_t(1)) * sizeof(gl::TransformObject)));

	_circles = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, std::max(plan.circles, uint32_t(1)) * sizeof(gl::Circle2DIndex)));

	if (!_vertexes || !_indexes || !_transforms || !_circles) {
		return false;
	}

	ShadowBufferMap vertexesMap(_vertexes, fhandle.isPersistentMapping());
	ShadowBufferMap indexesMap(_indexes, fhandle.isPersistentMapping());
	ShadowBufferMap transformMap(_transforms, fhandle.isPersistentMapping());
	ShadowBufferMap circlesMap(_circles, fhandle.isPersistentMapping());

	gl::TransformObject val;
	memcpy(transformMap.region.ptr, &val, sizeof(gl::TransformObject));

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = sizeof(gl::TransformObject);
	uint32_t circleOffset = 0;

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;
	uint32_t transformIdx = 1;

	auto pushVertexes = [&] (const gl::CmdShadow *cmd, const gl::TransformObject &transform, gl::VertexData *vertexes) {
		auto target = (Vec4 *)vertexesMap.region.ptr + vertexOffset;

		memcpy(transformMap.region.ptr + transtormOffset, &transform, sizeof(gl::TransformObject));

		for (size_t idx = 0; idx < vertexes->data.size(); ++ idx) {
			memcpy(target++, &vertexes->data[idx].pos, sizeof(Vec4));
		}

		auto indexTarget = (gl::Triangle2DIndex *)indexesMap.region.ptr + indexOffset;

		for (size_t idx = 0; idx < vertexes->indexes.size() / 3; ++ idx) {
			gl::Triangle2DIndex data({
				vertexes->indexes[idx * 3 + 0] + vertexOffset,
				vertexes->indexes[idx * 3 + 1] + vertexOffset,
				vertexes->indexes[idx * 3 + 2] + vertexOffset,
				transformIdx,
				cmd->value,
				1.0f
			});
			memcpy(indexTarget++, &data, sizeof(gl::Triangle2DIndex));

			_maxValue = std::max(_maxValue, cmd->value);
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size() / 3;
		transtormOffset += sizeof(gl::TransformObject);
		++ transformIdx;

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
	};

	for (auto &cmd : plan.commands) {
		for (auto &iit : cmd.vertexes) {
			pushVertexes(cmd.cmd, iit.mat, iit.data.get());
		}
	}

	auto pushSdf = [&] (const gl::CmdSdfGroup2D *cmd) {
		auto target = (Vec4 *)vertexesMap.region.ptr + vertexOffset;
		gl::TransformObject transform(cmd->modelTransform.getInversed());
		cmd->modelTransform.getScale(&val.shadow.x);

		memcpy(transformMap.region.ptr + transtormOffset, &transform, sizeof(gl::TransformObject));

		for (auto &it : cmd->data) {
			switch (it.type) {
			case gl::SdfShape::Circle2D: {
				auto data = (gl::SdfCircle2D *)it.bytes.data();
				auto pos = Vec4(data->origin, 0.0f, 1.0f);

				memcpy(target++, &pos, sizeof(Vec4));

				gl::Circle2DIndex index;
				index.origin = vertexOffset;
				index.transform = transtormOffset;
				index.value = cmd->value;
				index.opacity = cmd->opacity;

				memcpy(((gl::Circle2DIndex *)circlesMap.region.ptr) + circleOffset, &index, sizeof(gl::Circle2DIndex));

				++ circleOffset;
				++ vertexOffset;
				break;
			}
			case gl::SdfShape::Triangle2D:
				break;
			default: break;
			}
		}

		transtormOffset += sizeof(gl::TransformObject);
		++ transformIdx;
	};

	for (auto &cmd : plan.sdfCommands) {
		pushSdf(cmd);
	}

	_trianglesCount = plan.indexes / 3;
	_circlesCount = plan.circles;

	return true;
}

ShadowTrianglesAttachmentHandle::~ShadowTrianglesAttachmentHandle() { }

void ShadowTrianglesAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, const gl::glsl::ShadowData &data) {
	auto trianglesCount = std::max(uint32_t(1), data.trianglesCount);
	auto &pool = devFrame->getMemPool(devFrame);
	_triangles = pool->spawn(AllocationUsage::DeviceLocal, gl::BufferInfo(gl::BufferUsage::StorageBuffer,
			trianglesCount * sizeof(gl::Triangle2DData)));
	_circles = pool->spawn(AllocationUsage::DeviceLocal, gl::BufferInfo(gl::BufferUsage::StorageBuffer,
			data.circlesCount * sizeof(gl::Triangle2DData)));
	_gridSize = pool->spawn(AllocationUsage::DeviceLocal, gl::BufferInfo(gl::BufferUsage::StorageBuffer,
			data.gridWidth * data.gridHeight * 2 * sizeof(uint32_t)));
	_gridIndex = pool->spawn(AllocationUsage::DeviceLocal, gl::BufferInfo(gl::BufferUsage::StorageBuffer,
			(trianglesCount + data.circlesCount)* data.gridWidth * data.gridHeight * sizeof(uint32_t)));
}

bool ShadowTrianglesAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool isExternal) const {
	switch (idx) {
	case 0: return _triangles; break;
	case 1: return _gridSize; break;
	case 2: return _gridIndex; break;
	case 3: return _circles; break;
	default:
		break;
	}
	return false;
}

bool ShadowTrianglesAttachmentHandle::writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &info) {
	switch (info.index) {
	case 0:
		info.buffer = _triangles;
		info.offset = 0;
		info.range = _triangles->getSize();
		return true;
		break;
	case 1:
		info.buffer = _gridSize;
		info.offset = 0;
		info.range = _gridSize->getSize();
		return true;
		break;
	case 2:
		info.buffer = _gridIndex;
		info.offset = 0;
		info.range = _gridIndex->getSize();
		return true;
		break;
	case 3:
		info.buffer = _circles;
		info.offset = 0;
		info.range = _circles->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}


ShadowLightDataAttachment::~ShadowLightDataAttachment() { }

bool ShadowLightDataAttachment::init(StringView name) {
	if (BufferAttachment::init(name, gl::BufferInfo(gl::BufferUsage::UniformBuffer, size_t(sizeof(LightData))))) {
		return true;
	}
	return false;
}

bool ShadowLightDataAttachment::validateInput(const Rc<gl::AttachmentInputData> &data) const {
	if (dynamic_cast<gl::ShadowLightInput *>(data.get())) {
		return true;
	}
	return false;
}

auto ShadowLightDataAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowLightDataAttachmentHandle>::create(this, handle);
}

ShadowVertexAttachment::~ShadowVertexAttachment() { }

bool ShadowVertexAttachment::init(StringView name) {
	if (BufferAttachment::init(name, gl::BufferInfo(gl::BufferUsage::StorageBuffer))) {
		return true;
	}
	return false;
}

bool ShadowVertexAttachment::validateInput(const Rc<gl::AttachmentInputData> &data) const {
	if (dynamic_cast<gl::CommandList *>(data.get())) {
		return true;
	}
	return false;
}

auto ShadowVertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowVertexAttachmentHandle>::create(this, handle);
}

ShadowTrianglesAttachment::~ShadowTrianglesAttachment() { }

bool ShadowTrianglesAttachment::init(StringView name) {
	if (BufferAttachment::init(name, gl::BufferInfo(gl::BufferUsage::StorageBuffer))) {
		return true;
	}
	return false;
}

auto ShadowTrianglesAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowTrianglesAttachmentHandle>::create(this, handle);
}

ShadowImageArrayAttachment::~ShadowImageArrayAttachment() { }

bool ShadowImageArrayAttachment::init(StringView name, Extent2 extent) {
	return ImageAttachment::init(name, gl::ImageInfo(
		extent,
		gl::ArrayLayers(config::MaxAmbientLights + config::MaxDirectLights),
		gl::ForceImageUsage(gl::ImageUsage::Storage | gl::ImageUsage::Sampled | gl::ImageUsage::TransferDst),
		gl::RenderPassType::Compute,
		gl::ImageFormat::R8_UNORM),
	ImageAttachment::AttachmentInfo{
		.initialLayout = renderqueue::AttachmentLayout::Undefined,
		.finalLayout = renderqueue::AttachmentLayout::ShaderReadOnlyOptimal,
		.clearOnLoad = false,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f)
	});
}

gl::ImageInfo ShadowImageArrayAttachment::getAttachmentInfo(const AttachmentHandle *a, Extent3 e) const {
	auto img = (const ShadowImageArrayAttachmentHandle *)a;
	return img->getImageInfo();
}

auto ShadowImageArrayAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowImageArrayAttachmentHandle>::create(this, handle);
}

ShadowSdfImageAttachment::~ShadowSdfImageAttachment() { }

bool ShadowSdfImageAttachment::init(StringView name, Extent2 extent) {
	return ImageAttachment::init(name, gl::ImageInfo(
		extent,
		gl::ForceImageUsage(gl::ImageUsage::Storage | gl::ImageUsage::Sampled | gl::ImageUsage::TransferDst | gl::ImageUsage::TransferSrc),
		gl::RenderPassType::Compute,
		gl::ImageFormat::R16G16_SFLOAT),
	ImageAttachment::AttachmentInfo{
		.initialLayout = renderqueue::AttachmentLayout::Undefined,
		.finalLayout = renderqueue::AttachmentLayout::ShaderReadOnlyOptimal,
		.clearOnLoad = false,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f)
	});
}

gl::ImageInfo ShadowSdfImageAttachment::getAttachmentInfo(const AttachmentHandle *a, Extent3 e) const {
	auto img = (const ShadowSdfImageAttachmentHandle *)a;
	return img->getImageInfo();
}

Rc<ShadowSdfImageAttachment::AttachmentHandle> ShadowSdfImageAttachment::makeFrameHandle(const FrameQueue &handle) {
	return Rc<ShadowSdfImageAttachmentHandle>::create(this, handle);
}

bool ShadowPass::makeDefaultRenderQueue(renderqueue::Queue::Builder &builder, Extent2 extent) {
	using namespace renderqueue;

	auto trianglesShader = builder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp);
	auto imageShader = builder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp);

	auto pass = Rc<vk::ShadowPass>::create("ShadowPass", RenderOrdering(0));
	builder.addRenderPass(pass);

	builder.addComputePipeline(pass, SdfTrianglesComp, trianglesShader);
	builder.addComputePipeline(pass, SdfImageComp, imageShader);

	auto lightDataInput = Rc<vk::ShadowLightDataAttachment>::create("ShadowLightDataAttachment");
	auto vertexInput = Rc<vk::ShadowVertexAttachment>::create("ShadowVertexAttachment");
	auto triangles = Rc<vk::ShadowTrianglesAttachment>::create("ShadowTrianglesAttachment");

	auto array = Rc<ShadowImageArrayAttachment>::create("Array", extent);

	builder.addPassInput(pass, 0, lightDataInput, AttachmentDependencyInfo());
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, triangles, AttachmentDependencyInfo());

	builder.addPassOutput(pass, 0, array, AttachmentDependencyInfo{
		PipelineStage::ComputeShader, AccessType::ShaderWrite,
		PipelineStage::ComputeShader, AccessType::ShaderWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::StorageImage, AttachmentLayout::ShaderReadOnlyOptimal);

	// define global input-output
	// materialInput is persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(lightDataInput);
	builder.addInput(vertexInput);
	builder.addInput(array);
	builder.addOutput(array);
	return true;
}

bool ShadowPass::init(StringView name, RenderOrdering ord) {
	return QueuePass::init(name, gl::RenderPassType::Compute, ord, 1);
}

auto ShadowPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<ShadowPassHandle>::create(*this, handle);
}

void ShadowPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<ShadowVertexAttachment *>(it->getAttachment())) {
			_vertexes = a;
		} else if (auto a = dynamic_cast<ShadowTrianglesAttachment *>(it->getAttachment())) {
			_triangles = a;
		} else if (auto a = dynamic_cast<ShadowLightDataAttachment *>(it->getAttachment())) {
			_lights = a;
		} else if (auto a = dynamic_cast<ShadowImageArrayAttachment *>(it->getAttachment())) {
			_array = a;
		}
	}
}

bool ShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (ShadowPass *)_renderPass.get();

	ShadowTrianglesAttachmentHandle *trianglesHandle = nullptr;
	ShadowLightDataAttachmentHandle *lightsHandle = nullptr;

	if (auto lightsBuffer = q.getAttachment(pass->getLights())) {
		_lightsBuffer = lightsHandle = (ShadowLightDataAttachmentHandle *)lightsBuffer->handle.get();
	}

	if (auto trianglesBuffer = q.getAttachment(pass->getTriangles())) {
		_trianglesBuffer = trianglesHandle = (ShadowTrianglesAttachmentHandle *)trianglesBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const ShadowVertexAttachmentHandle *)vertexBuffer->handle.get();
	}

	if (auto arrayAttachment = q.getAttachment(pass->getArray())) {
		_arrayAttachment = (const ShadowImageArrayAttachmentHandle *)arrayAttachment->handle.get();
	}

	if (lightsHandle && lightsHandle->getLightsCount()) {
		lightsHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
				_vertexBuffer->getTrianglesCount(), _vertexBuffer->getMaxValue(), _gridCellSize, q.getExtent());

		if (lightsHandle->getObjectsCount() > 0 && trianglesHandle) {
			trianglesHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()), lightsHandle->getShadowData());
		}

		return QueuePassHandle::prepare(q, move(cb));
	} else {
		cb(true);
		return true;
	}
}

Vector<const CommandBuffer *> ShadowPassHandle::doPrepareCommands(FrameHandle &h) {
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		auto pass = (RenderPassImpl *)_data->impl.get();

		pass->perform(*this, buf, [&] {
			auto arrayImage = (Image *)_arrayAttachment->getImage()->getImage().get();
			auto targetLayout = (_vertexBuffer && _vertexBuffer->getTrianglesCount()) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			ImageMemoryBarrier inImageBarriers[] = {
				ImageMemoryBarrier(arrayImage, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout)
			};

			if (!_vertexBuffer || _vertexBuffer->getTrianglesCount() == 0) {
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, inImageBarriers);
				buf.cmdClearColorImage(arrayImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4F::BLACK);

				auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

				if (_pool->getFamilyIdx() != gIdx) {
					BufferMemoryBarrier transferBufferBarrier(_lightsBuffer->getBuffer(),
						VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
						QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE);

					ImageMemoryBarrier transferImageBarrier(arrayImage,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
					arrayImage->setPendingBarrier(transferImageBarrier);

					buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
							makeSpanView(&transferBufferBarrier, 1), makeSpanView(&transferImageBarrier, 1));
				}
				return;
			}

			ComputePipeline *pipeline = nullptr;
			buf.cmdBindDescriptorSets(pass);
			buf.cmdFillBuffer(_trianglesBuffer->getGridSize(), 0);

			pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfTrianglesComp))->pipeline.get();
			buf.cmdBindPipeline(pipeline);

			BufferMemoryBarrier bufferBarrier(_trianglesBuffer->getGridSize(),
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
			);

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&bufferBarrier, 1));

			buf.cmdDispatch((_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1);

			BufferMemoryBarrier bufferBarriers[] = {
				BufferMemoryBarrier(_trianglesBuffer->getTriangles(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
				BufferMemoryBarrier(_trianglesBuffer->getGridSize(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
				BufferMemoryBarrier(_trianglesBuffer->getGridIndex(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
			};

			buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
					bufferBarriers, inImageBarriers);

			pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfImageComp))->pipeline.get();
			buf.cmdBindPipeline(pipeline);

			buf.cmdDispatch(
					(arrayImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
					(arrayImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1);

			// transfer image and buffer to transfer queue
			auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

			if (_pool->getFamilyIdx() != gIdx) {
				BufferMemoryBarrier transferBufferBarrier(_lightsBuffer->getBuffer(),
					VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx}, 0, VK_WHOLE_SIZE);

				ImageMemoryBarrier transferImageBarrier(arrayImage,
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					QueueFamilyTransfer{_pool->getFamilyIdx(), gIdx});
				arrayImage->setPendingBarrier(transferImageBarrier);

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
						makeSpanView(&transferBufferBarrier, 1), makeSpanView(&transferImageBarrier, 1));
			}
		});
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void ShadowImageArrayAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::ShadowLightInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		_shadowDensity = d->shadowDensity;
		_currentImageInfo = ((ImageAttachment *)_attachment.get())->getImageInfo();
		_currentImageInfo.arrayLayers = gl::ArrayLayers(d->ambientLightCount + d->directLightCount);
		_currentImageInfo.extent = Extent2(std::floor(_currentImageInfo.extent.width * _shadowDensity),
				std::floor(_currentImageInfo.extent.height * _shadowDensity));
		cb(true);
	});
}

bool ShadowImageArrayAttachmentHandle::isAvailable(const FrameQueue &) const {
	return _currentImageInfo.arrayLayers.get() > 0;
}

void ShadowSdfImageAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<gl::ShadowLightInput>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, d = move(d), cb = move(cb)] (FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		_shadowDensity = d->shadowDensity;
		_sceneDensity = d->sceneDensity;
		_currentImageInfo = ((ImageAttachment *)_attachment.get())->getImageInfo();
		_currentImageInfo.extent = Extent2(std::floor((_currentImageInfo.extent.width / d->sceneDensity) * _shadowDensity),
				std::floor((_currentImageInfo.extent.height / d->sceneDensity) * _shadowDensity));
		cb(true);
	});
}

}
