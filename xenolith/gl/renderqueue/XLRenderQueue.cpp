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

#include "XLDefine.h"
#include "spirv_reflect.h"

// Log events in frame emitter (frame submission/completition)
#define XL_FRAME_EMITTER_DEBUG 0

// Log FrameQueue attachments and render passes state changes
#define XL_FRAME_QUEUE_DEBUG 0

// Log FrameHandle events
#define XL_FRAME_DEBUG 0

#if XL_FRAME_EMITTER_DEBUG
#define XL_FRAME_EMITTER_LOG(...) log::vtext("FrameEmitter", __VA_ARGS__)
#else
#define XL_FRAME_EMITTER_LOG(...)
#endif

#if XL_FRAME_QUEUE_DEBUG
#define XL_FRAME_QUEUE_LOG(...) log::vtext("FrameQueue", "[", _queue->getName(), ": ",  _order, "] ", __VA_ARGS__)
#else
#define XL_FRAME_QUEUE_LOG(...)
#endif

#if XL_FRAME_DEBUG
#define XL_FRAME_LOG(...) log::vtext("Frame", __VA_ARGS__)
#else
#define XL_FRAME_LOG(...)
#endif

#define XL_FRAME_PROFILE(fn, tag, max) \
	do { XL_PROFILE_BEGIN(frame, "gl::FrameHandle", tag, max); fn; XL_PROFILE_END(frame); } while (0);

#include "XLRenderQueueAttachment.cc"
#include "XLRenderQueueResource.cc"
#include "XLRenderQueuePass.cc"
#include "XLRenderQueueQueue.cc"
#include "XLRenderQueueFrameCache.cc"
#include "XLRenderQueueFrameEmitter.cc"
#include "XLRenderQueueFrameHandle.cc"
#include "XLRenderQueueFrameQueue.cc"
#include "XLRenderQueueImageStorage.cc"

namespace stappler::xenolith::renderqueue {

uint32_t DependencyEvent::GetNextId() {
	static std::atomic<uint32_t> s_eventId = 1;
	return s_eventId.fetch_add(1);
}

void QueueData::clear() {
	for (auto &it : programs) {
		it->program = nullptr;
	}

	for (auto &it : passes) {
		for (auto &desc : it->descriptors) {
			desc->clear();
		}

		for (auto &subpass : it->subpasses) {
			for (auto &pipeline : subpass.graphicPipelines) {
				pipeline->pipeline = nullptr;
			}
			for (auto &pipeline : subpass.computePipelines) {
				pipeline->pipeline = nullptr;
			}
		}

		it->renderPass->invalidate();
		it->renderPass = nullptr;
		it->impl = nullptr;
	}

	for (auto &it : attachments) {
		it->clear();
	}

	if (resource) {
		resource->clear();
		resource = nullptr;
	}
	linked.clear();
	compiled = false;

	if (releaseCallback) {
		releaseCallback();
		releaseCallback = nullptr;
	}
}

StringView getDescriptorTypeName(DescriptorType type) {
	switch (type) {
	case DescriptorType::Sampler: return StringView("Sampler"); break;
	case DescriptorType::CombinedImageSampler: return StringView("CombinedImageSampler"); break;
	case DescriptorType::SampledImage: return StringView("SampledImage"); break;
	case DescriptorType::StorageImage: return StringView("StorageImage"); break;
	case DescriptorType::UniformTexelBuffer: return StringView("UniformTexelBuffer"); break;
	case DescriptorType::StorageTexelBuffer: return StringView("StorageTexelBuffer"); break;
	case DescriptorType::UniformBuffer: return StringView("UniformBuffer"); break;
	case DescriptorType::StorageBuffer: return StringView("StorageBuffer"); break;
	case DescriptorType::UniformBufferDynamic: return StringView("UniformBufferDynamic"); break;
	case DescriptorType::StorageBufferDynamic: return StringView("StorageBufferDynamic"); break;
	case DescriptorType::InputAttachment: return StringView("InputAttachment"); break;
	default: break;
	}
	return StringView("Unknown");
}

String getProgramStageDescription(ProgramStage fmt) {
	StringStream stream;
	if ((fmt & ProgramStage::Vertex) != ProgramStage::None) { stream << " Vertex"; }
	if ((fmt & ProgramStage::TesselationControl) != ProgramStage::None) { stream << " TesselationControl"; }
	if ((fmt & ProgramStage::TesselationEvaluation) != ProgramStage::None) { stream << " TesselationEvaluation"; }
	if ((fmt & ProgramStage::Geometry) != ProgramStage::None) { stream << " Geometry"; }
	if ((fmt & ProgramStage::Fragment) != ProgramStage::None) { stream << " Fragment"; }
	if ((fmt & ProgramStage::Compute) != ProgramStage::None) { stream << " Compute"; }
	if ((fmt & ProgramStage::RayGen) != ProgramStage::None) { stream << " RayGen"; }
	if ((fmt & ProgramStage::AnyHit) != ProgramStage::None) { stream << " AnyHit"; }
	if ((fmt & ProgramStage::ClosestHit) != ProgramStage::None) { stream << " ClosestHit"; }
	if ((fmt & ProgramStage::MissHit) != ProgramStage::None) { stream << " MissHit"; }
	if ((fmt & ProgramStage::AnyHit) != ProgramStage::None) { stream << " AnyHit"; }
	if ((fmt & ProgramStage::Intersection) != ProgramStage::None) { stream << " Intersection"; }
	if ((fmt & ProgramStage::Callable) != ProgramStage::None) { stream << " Callable"; }
	if ((fmt & ProgramStage::Task) != ProgramStage::None) { stream << " Task"; }
	if ((fmt & ProgramStage::Mesh) != ProgramStage::None) { stream << " Mesh"; }
	return stream.str();
}

void ProgramData::inspect(SpanView<uint32_t> data) {
	SpvReflectShaderModule shader;

	spvReflectCreateShaderModule(data.size() * sizeof(uint32_t), data.data(), &shader);

	switch (shader.spirv_execution_model) {
	case SpvExecutionModelVertex: stage = ProgramStage::Vertex; break;
	case SpvExecutionModelTessellationControl: stage = ProgramStage::TesselationControl; break;
	case SpvExecutionModelTessellationEvaluation: stage = ProgramStage::TesselationEvaluation; break;
	case SpvExecutionModelGeometry: stage = ProgramStage::Geometry; break;
	case SpvExecutionModelFragment: stage = ProgramStage::Fragment; break;
	case SpvExecutionModelGLCompute: stage = ProgramStage::Compute; break;
	case SpvExecutionModelKernel: stage = ProgramStage::Compute; break;
	case SpvExecutionModelTaskNV: stage = ProgramStage::Task; break;
	case SpvExecutionModelMeshNV: stage = ProgramStage::Mesh; break;
	case SpvExecutionModelRayGenerationKHR: stage = ProgramStage::RayGen; break;
	case SpvExecutionModelIntersectionKHR: stage = ProgramStage::Intersection; break;
	case SpvExecutionModelAnyHitKHR: stage = ProgramStage::AnyHit; break;
	case SpvExecutionModelClosestHitKHR: stage = ProgramStage::ClosestHit; break;
	case SpvExecutionModelMissKHR: stage = ProgramStage::MissHit; break;
	case SpvExecutionModelCallableKHR: stage = ProgramStage::Callable; break;
	default: break;
	}

	bindings.reserve(shader.descriptor_binding_count);
	for (auto &it : makeSpanView(shader.descriptor_bindings, shader.descriptor_binding_count)) {
		bindings.emplace_back(ProgramDescriptorBinding({it.set, it.binding, DescriptorType(it.descriptor_type), it.count}));
	}

	constants.reserve(shader.push_constant_block_count);
	for (auto &it : makeSpanView(shader.push_constant_blocks, shader.push_constant_block_count)) {
		constants.emplace_back(ProgramPushConstantBlock({it.absolute_offset, it.padded_size}));
	}

	entryPoints.reserve(shader.entry_point_count);
	for (auto &it : makeSpanView(shader.entry_points, shader.entry_point_count)) {
		entryPoints.emplace_back(ProgramEntryPointBlock({it.id, memory::string(it.name), it.local_size.x, it.local_size.y, it.local_size.z}));
	}

	spvReflectDestroyShaderModule(&shader);
}

SpecializationInfo::SpecializationInfo(const ProgramData *data) : data(data) { }

SpecializationInfo::SpecializationInfo(const ProgramData *data, SpanView<PredefinedConstant> c)
: data(data), constants(c.vec<memory::PoolInterface>()) { }

bool GraphicPipelineInfo::isSolid() const {
	if (material.getDepthInfo().writeEnabled || !material.getBlendInfo().enabled) {
		return true;
	}
	return false;
}

}
