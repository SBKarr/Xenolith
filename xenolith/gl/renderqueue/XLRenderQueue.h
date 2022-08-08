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
class Pipeline;

class Loop;
class View;

}

namespace stappler::xenolith::renderqueue {

struct PassData;

class FrameQueue;
class FrameEmitter;
class FrameHandle;
class FrameRequest;

class Attachment;
class AttachmentDescriptor;
class AttachmentRef;
class AttachmentHandle;

class BufferAttachment;
class BufferAttachmentDescriptor;
class BufferAttachmentRef;

class ImageAttachment;
class ImageAttachmentDescriptor;
class ImageAttachmentRef;

class Pass;
class PassHandle;
class Resource;

struct QueueData;
class Queue;

struct FrameAttachmentData;
struct FramePassData;

class ImageStorage;

struct ProgramDescriptorBinding {
	uint32_t set = 0;
	uint32_t descriptor = 0;
	DescriptorType type = DescriptorType::Unknown;
};

struct ProgramPushConstantBlock {
	uint32_t offset = 0;
	uint32_t size = 0;
};

struct ProgramInfo : NamedMem {
	ProgramStage stage;
	memory::vector<ProgramDescriptorBinding> bindings;
	memory::vector<ProgramPushConstantBlock> constants;
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

	SpecializationInfo(const ProgramData *);
	SpecializationInfo(const ProgramData *, SpanView<PredefinedConstant>);
};

struct PipelineInfo : NamedMem {
	memory::vector<SpecializationInfo> shaders;
	DynamicState dynamicState = DynamicState::Default;
	PipelineMaterialInfo material;

	bool isSolid() const;
};

struct PipelineData : PipelineInfo {
	const Pass *renderPass = nullptr;
	Rc<gl::Pipeline> pipeline; // GL implementation-dependent object
	uint32_t subpass = 0;
};

struct PipelineDescriptor {
	StringView name; // for external descriptors
	Attachment *attachment = nullptr;
	AttachmentDescriptor *descriptor = nullptr;
	DescriptorType type = DescriptorType::Unknown;
	ProgramStage stages = ProgramStage::None;
	uint32_t count = 1;
	uint32_t maxCount = 1;
	bool updateAfterBind = false;
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

struct SubpassData {
	SubpassData() = default;
	SubpassData(const SubpassData &) = default;
	SubpassData(SubpassData &&) = default;

	SubpassData &operator=(const SubpassData &) = delete;
	SubpassData &operator=(SubpassData &&) = delete;

	uint32_t index = 0;
	PassData *renderPass = nullptr;

	HashTable<PipelineData *> pipelines;
	memory::vector<BufferAttachmentRef *> inputBuffers;
	memory::vector<BufferAttachmentRef *> outputBuffers;

	memory::vector<AttachmentRef *> inputGenerics;
	memory::vector<AttachmentRef *> outputGenerics;

	memory::vector<ImageAttachmentRef *> inputImages;
	memory::vector<ImageAttachmentRef *> outputImages;
	memory::vector<ImageAttachmentRef *> resolveImages;
	ImageAttachmentRef *depthStencil = nullptr;
	memory::vector<uint32_t> preserve;
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

	memory::vector<AttachmentDescriptor *> descriptors;
	memory::vector<SubpassData> subpasses;
	memory::vector<SubpassDependency> dependencies;
	memory::vector<const PipelineDescriptor *> queueDescriptors;
	memory::vector<PipelineDescriptor> extraDescriptors;

	RenderOrdering ordering = RenderOrderingLowest;
	bool usesSamplers = false;
	bool hasUpdateAfterBind = false;

	Rc<Pass> renderPass;
	Rc<gl::RenderPass> impl;
};

StringView getDescriptorTypeName(DescriptorType);
String getProgramStageDescription(ProgramStage fmt);

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUE_H_ */
