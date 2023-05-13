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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUE_H_

#include "XLRenderQueueEnum.h"
#include "SPHashTable.h"

namespace stappler::xenolith::gl {

class RenderPass;
class Shader;
class GraphicPipeline;
class ComputePipeline;

class Loop;
class View;

}

namespace stappler::xenolith::renderqueue {

struct PassData;
struct SubpassData;

class FrameQueue;
class FrameEmitter;
class FrameHandle;
class FrameRequest;

class Attachment;
class AttachmentHandle;

class BufferAttachment;
class ImageAttachment;
class Pass;
class PassHandle;
class Resource;

struct QueueData;
class Queue;

struct FrameAttachmentData;
struct FramePassData;

struct AttachmentData;
struct AttachmentPassData;
struct PipelineLayoutData;
struct DescriptorSetData;

class ImageStorage;

class AttachmentBuilder;

struct ProgramDescriptorBinding {
	uint32_t set = 0;
	uint32_t descriptor = 0;
	DescriptorType type = DescriptorType::Unknown;
	uint32_t count = 0;
};

struct ProgramPushConstantBlock {
	uint32_t offset = 0;
	uint32_t size = 0;
};

struct ProgramEntryPointBlock {
	uint32_t id;
	memory::string name;
	uint32_t localX;
	uint32_t localY;
	uint32_t localZ;
};

struct ProgramInfo : NamedMem {
	ProgramStage stage;
	memory::vector<ProgramDescriptorBinding> bindings;
	memory::vector<ProgramPushConstantBlock> constants;
	memory::vector<ProgramEntryPointBlock> entryPoints;
};

struct ProgramData : ProgramInfo {
	using DataCallback = memory::callback<void(SpanView<uint32_t>)>;

	SpanView<uint32_t> data;
	memory::function<void(const DataCallback &)> callback = nullptr;
	Rc<gl::Shader> program; // GL implementation-dependent object

	void inspect(SpanView<uint32_t>);
};

struct SpecializationInfo {
	const ProgramData *data = nullptr;
	memory::vector<PredefinedConstant> constants;

	SpecializationInfo() = default;
	SpecializationInfo(const ProgramData *);
	SpecializationInfo(const ProgramData *, SpanView<PredefinedConstant>);
};

struct GraphicPipelineInfo : NamedMem {
	memory::vector<SpecializationInfo> shaders;
	DynamicState dynamicState = DynamicState::Default;
	PipelineMaterialInfo material;

	bool isSolid() const;
};

struct GraphicPipelineData : GraphicPipelineInfo {
	const SubpassData *subpass = nullptr;
	const PipelineLayoutData *layout = nullptr;
	Rc<gl::GraphicPipeline> pipeline; // GL implementation-dependent object
};

struct ComputePipelineInfo : NamedMem {
	SpecializationInfo shader;
};

struct ComputePipelineData : ComputePipelineInfo {
	const SubpassData *subpass = nullptr;
	const PipelineLayoutData *layout = nullptr;
	Rc<gl::ComputePipeline> pipeline; // GL implementation-dependent object
};

struct PipelineDescriptor : NamedMem {
	const DescriptorSetData *set = nullptr;
	const AttachmentPassData *attachment = nullptr;
	DescriptorType type = DescriptorType::Unknown;
	ProgramStage stages = ProgramStage::None;
	AttachmentLayout layout = AttachmentLayout::Ignored;
	uint32_t count = 1;
	uint32_t index = maxOf<uint32_t>();
	bool updateAfterBind = false;
	mutable uint64_t boundGeneration = 0;
};

struct SubpassDependency {
	static constexpr uint32_t External = maxOf<uint32_t>();

	uint32_t srcSubpass;
	PipelineStage srcStage;
	AccessType srcAccess;
	uint32_t dstSubpass;
	PipelineStage dstStage;
	AccessType dstAccess;
	bool byRegion;

	uint64_t value() const {
		return uint64_t(srcSubpass) << 32 | uint64_t(dstSubpass);
	}
};

inline bool operator < (const SubpassDependency &l, const SubpassDependency &r) { return l.value() < r.value(); }
inline bool operator == (const SubpassDependency &l, const SubpassDependency &r) { return l.value() == r.value(); }
inline bool operator != (const SubpassDependency &l, const SubpassDependency &r) { return l.value() != r.value(); }

struct AttachmentDependencyInfo {
	// when and how within renderpass/subpass attachment will be used for a first time
	PipelineStage initialUsageStage = PipelineStage::None;
	AccessType initialAccessMask = AccessType::None;

	// when and how within renderpass/subpass attachment will be used for a last time
	PipelineStage finalUsageStage = PipelineStage::None;
	AccessType finalAccessMask = AccessType::None;

	// FrameRenderPassState, after which attachment can be used on next renderpass
	// Or Initial if no dependencies
	FrameRenderPassState requiredRenderPassState = FrameRenderPassState::Initial;

	// FrameRenderPassState that can be processed before attachment is acquired
	FrameRenderPassState lockedRenderPassState = FrameRenderPassState::Initial;
};

struct AttachmentSubpassData : NamedMem {
	const AttachmentPassData *pass = nullptr;
	const SubpassData *subpass = nullptr;
	AttachmentLayout layout = AttachmentLayout::Ignored;
	AttachmentUsage usage = AttachmentUsage::None;
	AttachmentOps ops = AttachmentOps::Undefined;
	AttachmentDependencyInfo dependency;
};

struct AttachmentPassData : NamedMem {
	const AttachmentData *attachment = nullptr;
	const PassData *pass = nullptr;

	mutable uint32_t index = maxOf<uint32_t>();

	AttachmentOps ops = AttachmentOps::Undefined;

	// calculated initial layout
	// for first descriptor in execution chain - initial layout of queue's attachment or first usage layout
	// for others - final layout of previous descriptor in chain of execution
	AttachmentLayout initialLayout = AttachmentLayout::Undefined;

	// calculated final layout
	// for last descriptor in execution chain - final layout of queue's attachment or last usage layout
	// for others - last usage layout
	AttachmentLayout finalLayout = AttachmentLayout::Undefined;

	AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp storeOp = AttachmentStoreOp::DontCare;
	AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::DontCare;

	ColorMode colorMode;
	AttachmentDependencyInfo dependency;

	memory::vector<PipelineDescriptor *> descriptors;
	memory::vector<AttachmentSubpassData *> subpasses;
};

struct AttachmentData : NamedMem {
	const QueueData *queue = nullptr;
	bool transient = false;
	AttachmentOps ops = AttachmentOps::Undefined;
	AttachmentType type = AttachmentType::Image;
	AttachmentUsage usage = AttachmentUsage::None;
	memory::vector<AttachmentPassData *> passes;
	Rc<Attachment> attachment;
};

struct DescriptorSetData : NamedMem {
	const PipelineLayoutData *layout = nullptr;
	uint32_t index = 0;
	memory::vector<PipelineDescriptor *> descriptors;
};

struct PipelineLayoutData : NamedMem {
	const PassData *pass = nullptr;
	uint32_t index = 0;
	bool usesTextureSet = false;
	memory::vector<DescriptorSetData *> sets;
	memory::vector<const GraphicPipelineData *> graphicPipelines;
	memory::vector<const ComputePipelineData *> computePipelines;
};

struct SubpassData : NamedMem {
	SubpassData() = default;
	SubpassData(const SubpassData &) = default;
	SubpassData(SubpassData &&) = default;

	SubpassData &operator=(const SubpassData &) = delete;
	SubpassData &operator=(SubpassData &&) = delete;

	const PassData *pass = nullptr;
	uint32_t index = 0;

	HashTable<GraphicPipelineData *> graphicPipelines;
	HashTable<ComputePipelineData *> computePipelines;

	memory::vector<const AttachmentSubpassData *> inputImages;
	memory::vector<const AttachmentSubpassData *> outputImages;
	memory::vector<const AttachmentSubpassData *> resolveImages;
	const AttachmentSubpassData *depthStencil = nullptr;
	mutable memory::vector<uint32_t> preserve;
};

/** RenderOrdering defines order of execution for render passes between interdependent passes
 * if render passes is not interdependent, RenderOrdering can be used as an advice, or not used at all
 */
using RenderOrdering = ValueWrapper<uint32_t, class RenderOrderingFlag>;

static constexpr RenderOrdering RenderOrderingLowest = RenderOrdering::min();
static constexpr RenderOrdering RenderOrderingHighest = RenderOrdering::max();

struct PassData : NamedMem {
	PassData() = default;
	PassData(const PassData &) = default;
	PassData(PassData &&) = default;

	PassData &operator=(const PassData &) = delete;
	PassData &operator=(PassData &&) = delete;

	const QueueData *queue = nullptr;
	memory::vector<const AttachmentPassData *> attachments;
	memory::vector<const SubpassData *> subpasses;
	memory::vector<const PipelineLayoutData *> pipelineLayouts;
	memory::vector<SubpassDependency> dependencies;

	PassType type = PassType::Graphics;
	RenderOrdering ordering = RenderOrderingLowest;
	bool hasUpdateAfterBind = false;

	Rc<Pass> renderPass;
	Rc<gl::RenderPass> impl;
};

StringView getDescriptorTypeName(DescriptorType);
String getProgramStageDescription(ProgramStage fmt);

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUE_H_ */
