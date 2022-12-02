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

class ShadowVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowVertexAttachmentHandle();

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

	bool empty() const;

	uint32_t getTrianglesCount() const { return _trianglesCount; }

protected:
	virtual bool loadVertexes(FrameHandle &, const Rc<gl::CommandList> &);

	Rc<DeviceBuffer> _indexes;
	Rc<DeviceBuffer> _vertexes;
	Rc<DeviceBuffer> _transforms;
	Vector<gl::VertexSpan> _spans;
	uint32_t _trianglesCount = 0;
};

class ShadowTrianglesAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowTrianglesAttachmentHandle();

	void allocateBuffer(DeviceFrameHandle *, uint32_t trianglesCount);

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
			uint32_t, bool, VkDescriptorBufferInfo &) override;

	const Rc<DeviceBuffer> &getTriangles() const { return _triangles; }

protected:
	Rc<DeviceBuffer> _triangles;
};

class ShadowImageAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~ShadowImageAttachmentHandle() { }

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const override;
	virtual bool writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
			uint32_t, bool, VkDescriptorImageInfo &) override;
};

class ShadowPassHandle : public QueuePassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &) override;

	const ShadowVertexAttachmentHandle *_vertexBuffer = nullptr;
	const ShadowTrianglesAttachmentHandle *_trianglesBuffer = nullptr;
	const ShadowImageAttachmentHandle *_imageAttachment = nullptr;
};

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
	_indexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, (globalWritePlan.indexes / 3) * sizeof(ShadowTrianglesAttachment::IndexData)));

	_vertexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(info, globalWritePlan.vertexes * sizeof(Vec4)));

	_transforms = handle->getMemPool()->spawn(AllocationUsage::DeviceLocalHostVisible,
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
		vertexData.resize(globalWritePlan.vertexes * sizeof(gl::Vertex_V4F_V4F_T2F2U));
		indexData.resize(globalWritePlan.indexes * sizeof(uint32_t));
		transformData.resize((globalWritePlan.transforms + 1) * sizeof(gl::TransformObject));

		vertexesMap.ptr = vertexData.data(); vertexesMap.size = vertexData.size();
		indexesMap.ptr = indexData.data(); indexesMap.size = indexData.size();
		transformMap.ptr = transformData.data(); transformMap.size = transformData.size();
	}

	gl::TransformObject val;
	memcpy(transformMap.ptr, &val, sizeof(gl::TransformObject));

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

void ShadowTrianglesAttachmentHandle::allocateBuffer(DeviceFrameHandle *devFrame, uint32_t trianglesCount) {
	_triangles = devFrame->getMemPool()->spawn(AllocationUsage::DeviceLocal,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, trianglesCount * sizeof(ShadowTrianglesAttachment::TriangleData)));
}

bool ShadowTrianglesAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	return _triangles;
}

bool ShadowTrianglesAttachmentHandle::writeDescriptor(const QueuePassHandle &, const PipelineDescriptor &,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = _triangles->getBuffer();
	info.offset = 0;
	info.range = _triangles->getSize();
	return true;
}

ShadowVertexAttachment::~ShadowVertexAttachment() { }

bool ShadowVertexAttachment::init(StringView name, const gl::BufferInfo &info) {
	if (BufferAttachment::init(name, info)) {
		return true;
	}
	return false;
}

auto ShadowVertexAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowVertexAttachmentHandle>::create(this, handle);
}

ShadowTrianglesAttachment::~ShadowTrianglesAttachment() { }

bool ShadowTrianglesAttachment::init(StringView name, const gl::BufferInfo &info) {
	if (BufferAttachment::init(name, info)) {
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
		gl::ImageFormat::R16_SFLOAT),
	ImageAttachment::AttachmentInfo{
		.initialLayout = renderqueue::AttachmentLayout::Undefined,
		.finalLayout = renderqueue::AttachmentLayout::General,
		.clearOnLoad = true,
		.clearColor = Color4F(1.0f, 0.0f, 0.0f, 0.0f),
		.frameSizeCallback = [] (const FrameQueue &frame) {
			return Extent3(frame.getExtent());
		}
	});
}

auto ShadowImageAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ShadowImageAttachmentHandle>::create(this, handle);
}

bool ShadowPass::makeDefaultRenderQueue(renderqueue::Queue::Builder &builder, Extent2 extent) {
	using namespace renderqueue;

	auto trianglesShader = builder.addProgramByRef("ShadowPass_SdfTrianglesComp", xenolith::shaders::SdfTrianglesComp);
	auto imageShader = builder.addProgramByRef("ShadowPass_SdfImageComp", xenolith::shaders::SdfImageComp);

	auto pass = Rc<vk::ShadowPass>::create("ShadowPass", RenderOrdering(0));
	builder.addRenderPass(pass);

	builder.addComputePipeline(pass, SdfTrianglesComp, trianglesShader);
	builder.addComputePipeline(pass, SdfImageComp, imageShader);

	auto vertexInput = Rc<vk::ShadowVertexAttachment>::create("ShadowVertexAttachment",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer));
	auto triangles = Rc<vk::ShadowTrianglesAttachment>::create("ShadowTrianglesAttachment",
			gl::BufferInfo(gl::BufferUsage::StorageBuffer));

	auto sdf = Rc<ShadowImageAttachment>::create("Sdf", extent);

	builder.addPassInput(pass, 0, vertexInput, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, triangles, AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, sdf, AttachmentDependencyInfo{
		PipelineStage::ComputeShader, AccessType::SharedWrite,
		PipelineStage::ComputeShader, AccessType::SharedWrite,

		// can be reused after RenderPass is submitted
		FrameRenderPassState::Submitted,
	}, DescriptorType::StorageImage);

	// define global input-output
	// materialInput is persistent between frames, only vertexes should be provided before rendering started
	builder.addInput(vertexInput);
	builder.addOutput(sdf);
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
		}
	}
}

bool ShadowPassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = (ShadowPass *)_renderPass.get();

	ShadowTrianglesAttachmentHandle *trianglesHandle = nullptr;

	if (auto trianglesBuffer = q.getAttachment(pass->getTriangles())) {
		_trianglesBuffer = trianglesHandle = (ShadowTrianglesAttachmentHandle *)trianglesBuffer->handle.get();
	}

	if (auto vertexBuffer = q.getAttachment(pass->getVertexes())) {
		_vertexBuffer = (const ShadowVertexAttachmentHandle *)vertexBuffer->handle.get();
	}

	if (auto imageAttachment = q.getAttachment(pass->getImage())) {
		_imageAttachment = (const ShadowImageAttachmentHandle *)imageAttachment->handle.get();
	}

	if (trianglesHandle) {
		trianglesHandle->allocateBuffer(static_cast<DeviceFrameHandle *>(q.getFrame().get()), _vertexBuffer->getTrianglesCount());
	}

	return QueuePassHandle::prepare(q, move(cb));
}

Vector<VkCommandBuffer> ShadowPassHandle::doPrepareCommands(FrameHandle &) {
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

		struct PushBlock {
			uint32_t trianglesCount;
			float bbOffset;
		} block;

		block.trianglesCount = _vertexBuffer->getTrianglesCount();
		block.bbOffset = 0.00f; // 5% of viewport

		table->vkCmdPushConstants(buf, pass->getPipelineLayout(),
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushBlock), &block);

		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfTrianglesComp))->pipeline.get();
		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());

		table->vkCmdDispatch(buf, (_vertexBuffer->getTrianglesCount() - 1) / pipeline->getLocalX() + 1, 1, 1);

		auto targetImage = (Image *)_imageAttachment->getImage()->getImage().get();
		VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		/*auto clearColor = ((ImageAttachment *)_imageAttachment->getAttachment().get())->getClearColor();

		VkClearColorValue color{clearColor.r, clearColor.g, clearColor.b, clearColor.a};

		table->vkCmdClearColorImage(buf, targetImage->getImage(), VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);*/

		VkImageMemoryBarrier inImageBarrier({
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			targetImage->getImage(), range
		});

		VkBufferMemoryBarrier bufferBarrier({
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			_trianglesBuffer->getTriangles()->getBuffer(), 0, VK_WHOLE_SIZE,
		});

		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				1, &bufferBarrier,
				1, &inImageBarrier);

		pipeline = (ComputePipeline *)_data->subpasses[0].computePipelines.get(StringView(ShadowPass::SdfImageComp))->pipeline.get();
		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());
		table->vkCmdDispatch(buf,
				(targetImage->getInfo().extent.width - 1) / pipeline->getLocalX() + 1,
				(targetImage->getInfo().extent.height - 1) / pipeline->getLocalY() + 1,
				1);
		_imageAttachment->getImage()->setLayout(renderqueue::AttachmentLayout::General);
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

}
