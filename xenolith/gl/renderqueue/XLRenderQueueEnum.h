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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEENUM_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEENUM_H_

#include "XLDefine.h"

namespace stappler::xenolith::renderqueue {

enum class FrameRenderPassState {
	Initial,
	Ready,
	Owned,
	ResourcesAcquired,
	Prepared,
	Submission,
	Submitted,
	Complete,
	Finalized,
};

enum class FrameAttachmentState {
	Initial,
	Setup,
	InputRequired,
	Ready,
	ResourcesPending,
	ResourcesAcquired,
	Detached, // resource ownership transferred out of Frame
	Complete,
	ResourcesReleased,
	Finalized,
};

enum class AttachmentType {
	Image,
	Buffer,
	Generic
};

// VkPipelineStageFlagBits
enum class PipelineStage {
	None,
	TopOfPipe = 0x00000001,
	DrawIndirect = 0x00000002,
	VertexInput = 0x00000004,
	VertexShader = 0x00000008,
	TesselationControl = 0x00000010,
	TesselationEvaluation = 0x00000020,
	GeometryShader = 0x00000040,
	FragmentShader = 0x00000080,
	EarlyFragmentTest = 0x00000100,
	LateFragmentTest = 0x00000200,
	ColorAttachmentOutput = 0x00000400,
	ComputeShader = 0x00000800,
	Transfer = 0x00001000,
	BottomOfPipe = 0x00002000,
	Host = 0x00004000,
	AllGraphics = 0x00008000,
	AllCommands = 0x00010000,
	TransformFeedback = 0x01000000,
	ConditionalRendering = 0x00040000,
	AccelerationStructureBuild = 0x02000000,
	RayTracingShader = 0x00200000,
	ShadingRateImage = 0x00400000,
	TaskShader = 0x00080000,
	MeshShader = 0x00100000,
	FragmentDensityProcess = 0x00800000,
	CommandPreprocess = 0x00020000,
};

SP_DEFINE_ENUM_AS_MASK(PipelineStage);

// VkAccessFlag
enum class AccessType {
	None,
    IndirectCommandRead = 0x00000001,
    IndexRead = 0x00000002,
    VertexAttributeRead = 0x00000004,
    UniformRead = 0x00000008,
    InputAttachmantRead = 0x00000010,
    ShaderRead = 0x00000020,
    SharedWrite = 0x00000040,
    ColorAttachmentRead = 0x00000080,
    ColorAttachmentWrite = 0x00000100,
    DepthStencilAttachmentRead = 0x00000200,
	DepthStencilAttachmentWrite = 0x00000400,
    TransferRead = 0x00000800,
    TransferWrite = 0x00001000,
    HostRead = 0x00002000,
    HostWrite = 0x00004000,
    MemoryRead = 0x00008000,
    MemoryWrite = 0x00010000,
	TransformFeedbackWrite = 0x02000000,
	TransformFeedbackCounterRead = 0x04000000,
	TransformFeedbackCounterWrite = 0x08000000,
	ConditionalRenderingRead = 0x00100000,
	ColorAttachmentReadNonCoherent = 0x00080000,
	AccelerationStructureRead = 0x00200000,
	AccelerationStructureWrite = 0x00400000,
	ShadingRateImageRead = 0x00800000,
	FragmentDensityMapRead = 0x01000000,
	CommandPreprocessRead = 0x00020000,
	CommandPreprocessWrite = 0x00040000,
};

SP_DEFINE_ENUM_AS_MASK(AccessType);

// read-write operations on attachment within passes
enum class AttachmentOps {
	Undefined,
	ReadColor = 1,
	ReadStencil = 2,
	WritesColor = 4,
	WritesStencil = 8
};

SP_DEFINE_ENUM_AS_MASK(AttachmentOps);

// VkAttachmentLoadOp
enum class AttachmentLoadOp {
	Load = 0,
	Clear = 1,
	DontCare = 2,
};

// VkAttachmentStoreOp
enum class AttachmentStoreOp {
	Store = 0,
	DontCare = 1,
};

// Attachment usage within subpasses
enum class AttachmentUsage {
	None,
	Input = 1,
	Output = 2,
	InputOutput = Input | Output,
	Resolve = 4,
	DepthStencil = 8,
	InputDepthStencil = Input | DepthStencil
};

SP_DEFINE_ENUM_AS_MASK(AttachmentUsage);

// VkDescriptorType
enum class DescriptorType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	StorageImage = 3,
	UniformTexelBuffer = 4,
	StorageTexelBuffer = 5,
	UniformBuffer = 6,
	StorageBuffer = 7,
	UniformBufferDynamic = 8,
	StorageBufferDynamic = 9,
	InputAttachment = 10,
	Unknown = maxOf<uint32_t>()
};

// mapping to VkShaderStageFlagBits
enum class ProgramStage {
	None,
	Vertex = 0x00000001,
	TesselationControl = 0x00000002,
	TesselationEvaluation = 0x00000004,
	Geometry = 0x00000008,
	Fragment = 0x00000010,
	Compute = 0x00000020,
	RayGen = 0x00000100,
	AnyHit = 0x00000200,
	ClosestHit = 0x00000400,
	MissHit = 0x00000800,
	Intersection = 0x00001000,
	Callable = 0x00002000,
	Task = 0x00000040,
	Mesh = 0x00000080,
};

SP_DEFINE_ENUM_AS_MASK(ProgramStage);

// mapping to VkImageLayout
enum class AttachmentLayout : uint32_t {
	Undefined = 0,
	General = 1,
	ColorAttachmentOptimal = 2,
	DepthStencilAttachmentOptimal = 3,
	DepthStencilReadOnlyOptimal = 4,
	ShaderReadOnlyOptimal = 5,
	TransferSrcOptimal = 6,
	TransferDstOptimal = 7,
	Preinitialized = 8,
	DepthReadOnlyStencilAttachmentOptimal = 1000117000,
	DepthAttachmentStencilReadOnlyOptimal = 1000117001,
	DepthAttachmentOptimal = 1000241000,
	DepthReadOnlyOptimal = 1000241001,
	StencilAttachmentOptimal = 1000241002,
	StencilReadOnlyOptimal = 1000241003,
	PresentSrc = 1000001002,
	Ignored = maxOf<uint32_t>()
};

enum class PassType {
	Graphics,
	Compute,
	Transfer,
	Generic
};

// engine-defined specialization constants for shaders
enum class PredefinedConstant {
	SamplersArraySize,
	SamplersDescriptorIdx,
	TexturesArraySize,
	TexturesDescriptorIdx,
};

enum class DynamicState {
	None,
	Viewport = 1,
	Scissor = 2,

	Default = Viewport | Scissor,
};

SP_DEFINE_ENUM_AS_MASK(DynamicState)

// dummy class for attachment input, made just to separate inputs from simple refs
struct AttachmentInputData : public Ref {
	virtual ~AttachmentInputData() { }

};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEENUM_H_ */
