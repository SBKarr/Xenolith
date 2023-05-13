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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEQUEUE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEQUEUE_H_

#include "XLRenderQueueAttachment.h"
#include "XLRenderQueuePass.h"
#include <typeindex>

namespace stappler::xenolith::renderqueue {

/* RenderQueue/RenderGraph implementation notes:
 *
 * 	- RenderQueue
 * 		- Attachment - global/per-queue data
 * 			- AttachmentDescriptor - per-pass attachment data
 * 				- AttachmentRef - per-subpass attachment data
 * 			- AttachmentHandle - per-frame attachment data
 * 		- RenderPass
 * 			- AttachmentDescriptor - pass attachments
 * 			- RenderSubpass
 * 				- AttachmentRef - subpass attachments
 * 			- RenderSubpassDependency - dependency between subpasses
 * 			- RenderPassHandle - per-frame pass data
 */

struct QueueData : NamedMem {
	memory::pool_t *pool = nullptr;
	memory::vector<AttachmentData *> input;
	memory::vector<AttachmentData *> output;
	HashTable<AttachmentData *> attachments;
	HashTable<PassData *> passes;
	HashTable<ProgramData *> programs;
	HashTable<GraphicPipelineData *> graphicPipelines;
	HashTable<ComputePipelineData *> computePipelines;
	HashTable<Rc<Resource>> linked;
	Function<void(FrameRequest &)> beginCallback;
	Function<void(FrameRequest &)> endCallback;
	Function<void()> releaseCallback;
	Rc<Resource> resource;
	bool compiled = false;
	uint64_t order = 0;

	memory::map<std::type_index, Attachment *> typedInput;
	memory::map<std::type_index, Attachment *> typedOutput;

	void clear();
};

class Queue : public NamedRef {
public:
	using FrameRequest = renderqueue::FrameRequest;
	using FrameQueue = renderqueue::FrameQueue;
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using FrameHandle = renderqueue::FrameHandle;
	using AttachmentData = renderqueue::AttachmentData;
	using AttachmentBuilder = renderqueue::AttachmentBuilder;

	class Builder;

	Queue();
	virtual ~Queue();

	virtual bool init(Builder &&);

	bool isCompiled() const;
	void setCompiled(bool, Function<void()> &&);

	bool isCompatible(const gl::ImageInfo &) const;

	virtual StringView getName() const override;

	const HashTable<ProgramData *> &getPrograms() const;
	const HashTable<PassData *> &getPasses() const;
	const HashTable<GraphicPipelineData *> &getGraphicPipelines() const;
	const HashTable<ComputePipelineData *> &getComputePipelines() const;
	const HashTable<AttachmentData *> &getAttachments() const;
	const HashTable<Rc<Resource>> &getLinkedResources() const;
	Rc<Resource> getInternalResource() const;

	const memory::vector<AttachmentData *> &getInputAttachments() const;
	const memory::vector<AttachmentData *> &getOutputAttachments() const;

	template <typename T>
	auto getInputAttachment() const -> const T *;

	template <typename T>
	auto getOutputAttachment() const -> const T *;

	const Attachment *getInputAttachment(std::type_index name) const;
	const Attachment *getOutputAttachment(std::type_index name) const;

	const PassData *getPass(StringView) const;
	const ProgramData *getProgram(StringView) const;
	const GraphicPipelineData *getGraphicPipeline(StringView) const;
	const ComputePipelineData *getComputePipeline(StringView) const;
	const AttachmentData *getAttachment(StringView) const;

	Vector<AttachmentData *> getOutput() const;
	Vector<AttachmentData *> getOutput(AttachmentType) const;
	AttachmentData *getPresentImageOutput() const;
	AttachmentData *getTransferImageOutput() const;

	// get next frame order dumber for this queue
	uint64_t incrementOrder();

	// Prepare queue to be used on target device
	bool prepare(gl::Device &);

	void beginFrame(FrameRequest &);
	void endFrame(FrameRequest &);

protected:
	QueueData *_data = nullptr;
};

class AttachmentBuilder final {
public:
	void setType(AttachmentType type);

	void defineAsInput(AttachmentOps ops = AttachmentOps::WritesColor | AttachmentOps::WritesStencil);
	void defineAsOutput(AttachmentOps ops = AttachmentOps::ReadColor | AttachmentOps::ReadStencil);

	const AttachmentData *getAttachmentData() const { return _data; }

protected:
	friend class Queue::Builder;

	AttachmentBuilder(AttachmentData *);

	AttachmentData *_data = nullptr;
};

class AttachmentPassBuilder final {
public:
	void setAttachmentOps(AttachmentOps);
	void setInitialLayout(AttachmentLayout);
	void setFinalLayout(AttachmentLayout);

	void setLoadOp(AttachmentLoadOp);
	void setStoreOp(AttachmentStoreOp);
	void setStencilLoadOp(AttachmentLoadOp);
	void setStencilStoreOp(AttachmentStoreOp);

	void setColorMode(const ColorMode &);
	void setDependency(const AttachmentDependencyInfo &);

protected:
	friend class PassBuilder;

	AttachmentPassBuilder(AttachmentPassData *);

	AttachmentPassData *_data = nullptr;
};

class DescriptorSetBuilder final {
public:
	bool addDescriptor(const AttachmentPassData *, DescriptorType = DescriptorType::Unknown, AttachmentLayout = AttachmentLayout::Ignored);

protected:
	friend class PipelineLayoutBuilder;

	DescriptorSetBuilder(DescriptorSetData *);

	DescriptorSetData *_data = nullptr;
};

class PipelineLayoutBuilder final {
public:
	bool addSet(const Callback<void(DescriptorSetBuilder &)> &);
	void setUsesTextureSet(bool);

protected:
	friend class PassBuilder;

	PipelineLayoutBuilder(PipelineLayoutData *);

	PipelineLayoutData *_data = nullptr;
};

class SubpassBuilder final {
public:
	bool addColor(const AttachmentPassData *, AttachmentDependencyInfo, AttachmentLayout = AttachmentLayout::Ignored,
			AttachmentOps = AttachmentOps::Undefined);
	bool addInput(const AttachmentPassData *, AttachmentDependencyInfo, AttachmentLayout = AttachmentLayout::Ignored,
			AttachmentOps = AttachmentOps::Undefined);

	bool addResolve(const AttachmentPassData *color, const AttachmentPassData *resolve,
			AttachmentDependencyInfo colorDep, AttachmentDependencyInfo resolveDep);

	bool setDepthStencil(const AttachmentPassData *, AttachmentDependencyInfo, AttachmentLayout = AttachmentLayout::Ignored,
			AttachmentOps = AttachmentOps::Undefined);

	template <typename ... Args>
	const GraphicPipelineData * addGraphicPipeline(StringView key, const PipelineLayoutData *layout, Args && ...args) {
		if (auto p = emplacePipeline(key, layout)) {
			if (setPipelineOptions(*p, std::forward<Args>(args)...)) {
				finalizePipeline(p);
				return p;
			}
			erasePipeline(p);
		}
		return nullptr;
	}

	const ComputePipelineData *addComputePipeline(StringView key, const PipelineLayoutData *layout, SpecializationInfo &&);

protected:
	friend class PassBuilder;

	GraphicPipelineData *emplacePipeline(StringView key, const PipelineLayoutData *);
	void finalizePipeline(GraphicPipelineData *);
	void erasePipeline(GraphicPipelineData *);

	bool setPipelineOption(GraphicPipelineData &f, DynamicState);
	bool setPipelineOption(GraphicPipelineData &f, const Vector<SpecializationInfo> &);
	bool setPipelineOption(GraphicPipelineData &f, const PipelineMaterialInfo &);

	template <typename T>
	bool setPipelineOptions(GraphicPipelineData &f, T && t) {
		return setPipelineOption(f, std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	bool setPipelineOptions(GraphicPipelineData &f, T && t, Args && ... args) {
		if (!setPipelineOption(f, std::forward<T>(t))) {
			return false;
		}
		return setPipelineOptions(f, std::forward<Args>(args)...);
	}

	SubpassBuilder(SubpassData *);

	SubpassData *_data;
};

class PassBuilder final {
public:
	const PipelineLayoutData * addDescriptorLayout(const Callback<void(PipelineLayoutBuilder &)> &);

	const SubpassData * addSubpass(const Callback<void(SubpassBuilder &)> &);

	bool addSubpassDependency(const SubpassData *src, PipelineStage srcStage, AccessType srcAccess,
			const SubpassData *dst, PipelineStage dstStage, AccessType dstAccess, bool byRegion = true);

	const AttachmentPassData *addAttachment(const AttachmentData *);
	const AttachmentPassData *addAttachment(const AttachmentData *, const Callback<void(AttachmentPassBuilder &)> &);

protected:
	friend class Queue::Builder;
	friend class Pass;

	PassData *getData() const { return _data; }

	PassBuilder(PassData *);

	PassData *_data = nullptr;
};

class Queue::Builder final {
public:
	Builder(StringView);
	~Builder();

	const AttachmentData *addAttachemnt(StringView name, const Callback<Rc<Attachment>(AttachmentBuilder &)> &);

	const PassData * addPass(StringView name, PassType, RenderOrdering, const Callback<Rc<Pass>(PassBuilder &)> &);

	// add program, copy all data
	const ProgramData * addProgram(StringView key, SpanView<uint32_t>, const ProgramInfo * = nullptr);

	// add program, take shader data by ref, data should exists for all resource lifetime
	const ProgramData * addProgramByRef(StringView key, SpanView<uint32_t>, const ProgramInfo * = nullptr);

	// add program, data will be acquired with callback when needed
	const ProgramData * addProgram(StringView key, const memory::function<void(const ProgramData::DataCallback &)> &,
			const ProgramInfo * = nullptr);

	// resources, that will be compiled with RenderQueue
	void setInternalResource(Rc<Resource> &&);

	// external resources, that should be compiled when added
	void addLinkedResource(const Rc<Resource> &);

	void setBeginCallback(Function<void(FrameRequest &)> &&);
	void setEndCallback(Function<void(FrameRequest &)> &&);

protected:
	memory::pool_t *getPool() const;

	const PassData *getPassData(const Rc<Pass> &) const;
	const SubpassData *getSubpassData(const Rc<Pass> &, uint32_t) const;

	friend class Queue;

	QueueData *_data = nullptr;
};


template <typename T>
inline auto Queue::getInputAttachment() const -> const T * {
	if (auto c = getInputAttachment(std::type_index(typeid(T)))) {
		return static_cast<const T *>(c);
	}

	return nullptr;
}

template <typename T>
inline auto Queue::getOutputAttachment() const -> const T * {
	if (auto c = getOutputAttachment(std::type_index(typeid(T)))) {
		return static_cast<const T *>(c);
	}

	return nullptr;
}

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEQUEUE_H_ */
