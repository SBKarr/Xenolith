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

		_input = move(d);
		cb(true);
	});
}

bool ShadowLightDataAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	return _data;
}

bool ShadowLightDataAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool, VkDescriptorBufferInfo &info) {
	switch (idx) {
	case 0:
		info.buffer = _data->getBuffer();
		info.offset = 0;
		info.range = _data->getSize();
		return true;
		break;
	default:
		break;
	}
	return false;
}

void ShadowLightDataAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, uint32_t trianglesCount, uint32_t gridSize, Extent2 extent) {
	_data = devFrame->getMemPool(devFrame)->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(((BufferAttachment *)_attachment.get())->getInfo(), sizeof(ShadowLightDataAttachment::LightData)));

	ShadowLightDataAttachment::LightData *data = nullptr;

	DeviceBuffer::MappedRegion mapped;
	if (devFrame->isPersistentMapping()) {
		mapped = _data->map();
		data = (ShadowLightDataAttachment::LightData *)mapped.ptr;
	} else {
		data = new ShadowLightDataAttachment::LightData;
	}

	data->trianglesCount = trianglesCount;
	data->gridSize = gridSize;
	data->gridWidth = (extent.width - 1) / gridSize + 1;
	data->gridHeight = (extent.height - 1) / gridSize + 1;
	data->ambientLightCount = _input->ambientLightCount;
	data->directLightCount = _input->directLightCount;
	data->bbOffset = getBoxOffset();
	data->pixX = 2.0f / extent.width;
	data->pixY = 2.0f / extent.height;
	memcpy(data->ambientLights, _input->ambientLights, sizeof(gl::AmbientLightData) * config::MaxAmbientLights);
	memcpy(data->directLights, _input->directLights, sizeof(gl::DirectLightData) * config::MaxDirectLights);

	if (devFrame->isPersistentMapping()) {
		_data->unmap(mapped, true);
	} else {
		_data->setData(BytesView((const uint8_t *)data, sizeof(ShadowLightDataAttachment::LightData)));
		delete data;
	}
}

float ShadowLightDataAttachmentHandle::getBoxOffset() const {
	float bbox = 0.0f;
	for (size_t i = 0; i < _input->ambientLightCount; ++ i) {
		auto &l = _input->ambientLights[i];
		float n_2 = l.normal.x * l.normal.x + l.normal.y * l.normal.y;
		float m = std::sqrt(n_2) / std::sqrt(1 - n_2);

		bbox = std::max(m + l.normal.w, bbox);
	}
	return bbox;
}

uint32_t ShadowLightDataAttachmentHandle::getLightsCount() const {
	return _input->ambientLightCount + _input->directLightCount;
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
	case 0:
		return _indexes;
		break;
	case 1:
		return _vertexes;
		break;
	case 2:
		return _transforms;
		break;
	default:
		break;
	}
	return false;
}

bool ShadowVertexAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool, VkDescriptorBufferInfo &info) {
	switch (idx) {
	case 0:
		info.buffer = _indexes->getBuffer();
		info.offset = 0;
		info.range = _indexes->getSize();
		return true;
		break;
	case 1:
		info.buffer = _vertexes->getBuffer();
		info.offset = 0;
		info.range = _vertexes->getSize();
		return true;
		break;
	case 2:
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

bool ShadowVertexAttachmentHandle::empty() const {
	return !_indexes || !_vertexes || !_transforms;
}

bool ShadowVertexAttachmentHandle::loadVertexes(FrameHandle &fhandle, const Rc<gl::CommandList> &commands) {
	auto handle = dynamic_cast<DeviceFrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	struct PlanCommandInfo {
		const gl::CmdShadow *cmd;
		SpanView<gl::TransformedVertexData> vertexes;
	};

	struct MaterialWritePlan {
		uint32_t vertexes = 0;
		uint32_t indexes = 0;
		uint32_t transforms = 0;
		Map<gl::StateId, std::forward_list<PlanCommandInfo>> states;
	};

	// fill write plan
	MaterialWritePlan globalWritePlan;

	auto emplaceWritePlan = [&] (const gl::Command *c, const gl::CmdShadow *cmd, SpanView<gl::TransformedVertexData> vertexes) {
		for (auto &iit : vertexes) {
			globalWritePlan.vertexes += iit.data->data.size();
			globalWritePlan.indexes += iit.data->indexes.size();
			++ globalWritePlan.transforms;
		}

		auto iit = globalWritePlan.states.find(cmd->state);
		if (iit == globalWritePlan.states.end()) {
			iit = globalWritePlan.states.emplace(cmd->state, std::forward_list<PlanCommandInfo>()).first;
		}

		iit->second.emplace_front(PlanCommandInfo{cmd, vertexes});
	};

	auto pushDeferred = [&] (const gl::Command *c, const gl::CmdShadowDeferred *cmd) {
		auto vertexes = cmd->deferred->getData().pdup(handle->getPool()->getPool());

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

		emplaceWritePlan(c, cmd, vertexes);
	};

	auto cmd = commands->getFirst();
	while (cmd) {
		switch (cmd->type) {
		case gl::CommandType::CommandGroup:
		case gl::CommandType::VertexArray:
		case gl::CommandType::Deferred:
			break;
		case gl::CommandType::ShadowArray:
			emplaceWritePlan(cmd, (const gl::CmdShadowArray *)cmd->data, ((const gl::CmdShadowArray *)cmd->data)->vertexes);
			break;
		case gl::CommandType::ShadowDeferred:
			pushDeferred(cmd, (const gl::CmdShadowDeferred *)cmd->data);
			break;
		}
		cmd = cmd->next;
	}

	if (globalWritePlan.vertexes == 0 || globalWritePlan.indexes == 0) {
		return true;
	}

	auto &info = ((BufferAttachment *)_attachment.get())->getInfo();

	// create buffers
	auto pool = handle->getMemPool(handle);
	_indexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, (globalWritePlan.indexes / 3) * sizeof(ShadowTrianglesAttachment::IndexData)));

	_vertexes = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, globalWritePlan.vertexes * sizeof(Vec4)));

	_transforms = pool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, (globalWritePlan.transforms + 1) * sizeof(Mat4)));

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

	Mat4 val;
	memcpy(transformMap.ptr, &val, sizeof(Mat4));

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t transtormOffset = sizeof(Mat4);

	uint32_t materialVertexes = 0;
	uint32_t materialIndexes = 0;
	uint32_t transformIdx = 1;

	auto pushVertexes = [&] (const gl::CmdShadow *cmd, const Mat4 &transform, gl::VertexData *vertexes) {
		auto target = (Vec4 *)vertexesMap.ptr + vertexOffset;

		memcpy(transformMap.ptr + transtormOffset, &transform, sizeof(Mat4));

		for (size_t idx = 0; idx < vertexes->data.size(); ++ idx) {
			memcpy(target++, &vertexes->data[idx].pos, sizeof(Vec4));
		}

		auto indexTarget = (ShadowTrianglesAttachment::IndexData *)indexesMap.ptr + indexOffset;

		for (size_t idx = 0; idx < vertexes->indexes.size() / 3; ++ idx) {
			ShadowTrianglesAttachment::IndexData data({
				vertexes->indexes[idx * 3 + 0] + vertexOffset,
				vertexes->indexes[idx * 3 + 1] + vertexOffset,
				vertexes->indexes[idx * 3 + 2] + vertexOffset,
				transformIdx,
				cmd->value,
				1.0f
			});
			memcpy(indexTarget++, &data, sizeof(ShadowTrianglesAttachment::IndexData));
		}

		vertexOffset += vertexes->data.size();
		indexOffset += vertexes->indexes.size() / 3;
		transtormOffset += sizeof(Mat4);
		++ transformIdx;

		materialVertexes += vertexes->data.size();
		materialIndexes += vertexes->indexes.size();
	};

	// optimize draw order, minimize switching pipeline, textureSet and descriptors
	Vector<const Pair<const gl::MaterialId, MaterialWritePlan> *> drawOrder;

	for (auto &state : globalWritePlan.states) {
		// split order on states
		materialVertexes = 0;
		materialIndexes = 0;

		for (auto &cmd : state.second) {
			for (auto &iit : cmd.vertexes) {
				pushVertexes(cmd.cmd, iit.mat, iit.data.get());
			}
		}

		_spans.emplace_back(gl::VertexSpan({ 0, materialIndexes, 1, indexOffset - materialIndexes, state.first}));
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

	_trianglesCount = globalWritePlan.indexes / 3;

	return true;
}

ShadowTrianglesAttachmentHandle::~ShadowTrianglesAttachmentHandle() { }

void ShadowTrianglesAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, uint32_t trianglesCount, uint32_t gridSize, Extent2 extent) {
	uint32_t width = (extent.width - 1) / gridSize + 1;
	uint32_t height = (extent.height - 1) / gridSize + 1;
	auto pool = devFrame->getMemPool(devFrame);
	_triangles = pool->spawn(AllocationUsage::DeviceLocal,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, trianglesCount * sizeof(ShadowTrianglesAttachment::TriangleData)));
	_gridSize = pool->spawn(AllocationUsage::DeviceLocal,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, width * height * sizeof(uint32_t)));
	_gridIndex = pool->spawn(AllocationUsage::DeviceLocal,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, trianglesCount * width * height * sizeof(uint32_t)));
}

bool ShadowTrianglesAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool isExternal) const {
	switch (idx) {
	case 0: return _triangles; break;
	case 1: return _gridSize; break;
	case 2: return _gridIndex; break;
	default:
		break;
	}
	return false;
}

bool ShadowTrianglesAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t idx, bool, VkDescriptorBufferInfo &info) {
	switch (idx) {
	case 0:
		info.buffer = _triangles->getBuffer();
		info.offset = 0;
		info.range = _triangles->getSize();
		return true;
		break;
	case 1:
		info.buffer = _gridSize->getBuffer();
		info.offset = 0;
		info.range = _gridSize->getSize();
		return true;
		break;
	case 2:
		info.buffer = _gridIndex->getBuffer();
		info.offset = 0;
		info.range = _gridIndex->getSize();
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

ShadowImageAttachment::~ShadowImageAttachment() { }

bool ShadowImageAttachment::init(StringView name, Extent2 extent) {
	return ImageAttachment::init(name, gl::ImageInfo(
		extent,
		gl::ForceImageUsage(gl::ImageUsage::Storage | gl::ImageUsage::TransferSrc),
		gl::RenderPassType::Compute,
		gl::ImageFormat::R16G16_SFLOAT),
	ImageAttachment::AttachmentInfo{
		.initialLayout = renderqueue::AttachmentLayout::Undefined,
		.finalLayout = renderqueue::AttachmentLayout::General,
		.clearOnLoad = false,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f),
		.frameSizeCallback = [] (const FrameQueue &frame) {
			return Extent3(frame.getExtent());
		}
	});
}

auto ShadowImageAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowImageAttachmentHandle>::create(this, handle);
}

ShadowImageArrayAttachment::~ShadowImageArrayAttachment() { }

bool ShadowImageArrayAttachment::init(StringView name, Extent2 extent) {
	return ImageAttachment::init(name, gl::ImageInfo(
		extent,
		gl::ArrayLayers(config::MaxAmbientLights + config::MaxDirectLights),
		gl::ForceImageUsage(gl::ImageUsage::Storage | gl::ImageUsage::Sampled),
		gl::RenderPassType::Compute,
		gl::ImageFormat::R8_UNORM),
	ImageAttachment::AttachmentInfo{
		.initialLayout = renderqueue::AttachmentLayout::Undefined,
		.finalLayout = renderqueue::AttachmentLayout::ShaderReadOnlyOptimal,
		.clearOnLoad = false,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f),
		.frameSizeCallback = [] (const FrameQueue &frame) {
			return Extent3(frame.getExtent());
		}
	});
}

gl::ImageInfo ShadowImageArrayAttachment::getAttachmentInfo(const AttachmentHandle *a, Extent3 e) const {
	auto img = (const ShadowImageArrayAttachmentHandle *)a;
	return img->getImageInfo();
}

auto ShadowImageArrayAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowImageArrayAttachmentHandle>::create(this, handle);
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

	auto sdf = Rc<ShadowImageAttachment>::create("Sdf", extent);
	auto array = Rc<ShadowImageArrayAttachment>::create("Array", extent);

	builder.addPassInput(pass, 0, lightDataInput, AttachmentDependencyInfo());
	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, triangles, AttachmentDependencyInfo());

	builder.addPassOutput(pass, 0, array, AttachmentDependencyInfo{
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
	// materialInput is persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(lightDataInput);
	builder.addInput(vertexInput);
	builder.addInput(array);
	builder.addOutput(sdf);
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
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<ShadowVertexAttachment *>(it->getAttachment())) {
			_vertexes = a;
		} else if (auto a = dynamic_cast<ShadowTrianglesAttachment *>(it->getAttachment())) {
			_triangles = a;
		} else if (auto a = dynamic_cast<ShadowImageAttachment *>(it->getAttachment())) {
			_image = a;
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

	if (auto imageAttachment = q.getAttachment(pass->getImage())) {
		_imageAttachment = (const ShadowImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (auto arrayAttachment = q.getAttachment(pass->getArray())) {
		_arrayAttachment = (const ShadowImageArrayAttachmentHandle *)arrayAttachment->handle.get();
	}

	if (trianglesHandle) {
		trianglesHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
				_vertexBuffer->getTrianglesCount(), _gridCellSize, q.getExtent());
	}

	if (lightsHandle) {
		lightsHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()),
				_vertexBuffer->getTrianglesCount(), _gridCellSize, q.getExtent());
	}

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<VkCommandBuffer> ShadowPassHandle::doPrepareCommands(FrameHandle &h) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto pass = (RenderPassImpl *)_data->impl.get();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	_data->impl.cast<RenderPassImpl>()->perform(*this, buf, [&] {
		ComputePipeline *pipeline = nullptr;
		table->vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_COMPUTE, pass->getPipelineLayout(), 0,
				pass->getDescriptorSets().size(), pass->getDescriptorSets().data(), 0, nullptr);

		table->vkCmdFillBuffer(buf, _trianglesBuffer->getGridSize()->getBuffer(), 0, VK_WHOLE_SIZE, 0);

		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfTrianglesComp))->pipeline.get();
		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());

		VkBufferMemoryBarrier bufferBarrier({
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			_trianglesBuffer->getGridSize()->getBuffer(), 0, VK_WHOLE_SIZE,
		});

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);

		table->vkCmdDispatch(buf, (_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1, 1, 1);

		auto targetImage = (Image *)_imageAttachment->getImage()->getImage().get();
		auto arrayImage = (Image *)_arrayAttachment->getImage()->getImage().get();
		VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};

		VkImageMemoryBarrier inImageBarriers[] = {
			VkImageMemoryBarrier{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				targetImage->getImage(), range
			}, VkImageMemoryBarrier{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				arrayImage->getImage(), range
			}
		};

		VkBufferMemoryBarrier bufferBarriers[] = {
			VkBufferMemoryBarrier{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_trianglesBuffer->getTriangles()->getBuffer(), 0, VK_WHOLE_SIZE
			}, VkBufferMemoryBarrier{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_trianglesBuffer->getGridSize()->getBuffer(), 0, VK_WHOLE_SIZE
			}, VkBufferMemoryBarrier{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				_trianglesBuffer->getGridIndex()->getBuffer(), 0, VK_WHOLE_SIZE
			}
		};

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				3, bufferBarriers,
				2, inImageBarriers);

		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfImageComp))->pipeline.get();
		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());
		table->vkCmdDispatch(buf,
				(targetImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
				(targetImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1,
				1);
		_imageAttachment->getImage()->setLayout(renderqueue::AttachmentLayout::General);

		// transfer image and buffer to transfer queue
		auto gIdx = _device->getQueueFamily(QueueOperations::Graphics)->index;

		if (_pool->getFamilyIdx() != gIdx) {
			VkBufferMemoryBarrier transferBufferBarrier({
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
				_pool->getFamilyIdx(), gIdx,
				_lightsBuffer->getBuffer()->getBuffer(), 0, VK_WHOLE_SIZE,
			});

			VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
			VkImageMemoryBarrier transferImageBarrier({
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				_pool->getFamilyIdx(), gIdx,
				arrayImage->getImage(), range
			});
			arrayImage->setPendingBarrier(transferImageBarrier);

			table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
					0, nullptr,
					1, &transferBufferBarrier,
					1, &transferImageBarrier);
		}
	});

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool ShadowImageAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const {
	return true;
}

bool ShadowImageAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &desc,
		uint32_t, bool, VkDescriptorImageInfo &target) {
	auto &image = _queueData->image;
	auto viewInfo = gl::ImageViewInfo(*(renderqueue::ImageAttachmentDescriptor *)desc.descriptor);
	if (auto view = image->getView(viewInfo)) {
		target.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		target.imageView = ((ImageView *)view.get())->getImageView();
		return true;
	}
	return false;
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

		_currentImageInfo = ((ImageAttachment *)_attachment.get())->getImageInfo();
		_currentImageInfo.arrayLayers = gl::ArrayLayers(d->ambientLightCount + d->directLightCount);
		cb(true);
	});
}

bool ShadowImageArrayAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const {
	return true;
}

bool ShadowImageArrayAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &desc,
		uint32_t, bool, VkDescriptorImageInfo &target) {
	auto &image = _queueData->image;
	auto viewInfo = gl::ImageViewInfo(*(renderqueue::ImageAttachmentDescriptor *)desc.descriptor, image->getInfo());
	if (auto view = image->getView(viewInfo)) {
		target.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		target.imageView = ((ImageView *)view.get())->getImageView();
		return true;
	}
	return false;
}

}
