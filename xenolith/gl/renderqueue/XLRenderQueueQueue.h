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
	memory::vector<Attachment *> input;
	memory::vector<Attachment *> output;
	HashTable<Rc<Attachment>> attachments;
	HashTable<PassData *> passes;
	HashTable<ProgramData *> programs;
	HashTable<PipelineData *> pipelines;
	HashTable<Rc<Resource>> linked;
	Function<void(FrameRequest &)> beginCallback;
	Function<void(FrameRequest &)> endCallback;
	Function<void()> releaseCallback;
	Rc<Resource> resource;
	bool compiled = false;
	uint64_t order = 0;

	void clear();
};

class Queue : public NamedRef {
public:
	using FrameRequest = renderqueue::FrameRequest;
	using FrameQueue = renderqueue::FrameQueue;
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using FrameHandle = renderqueue::FrameHandle;

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
	const HashTable<PipelineData *> &getPipelines() const;
	const HashTable<Rc<Attachment>> &getAttachments() const;
	const HashTable<Rc<Resource>> &getLinkedResources() const;
	Rc<Resource> getInternalResource() const;

	const memory::vector<Attachment *> &getInputAttachments() const;
	const memory::vector<Attachment *> &getOutputAttachments() const;

	const PassData *getPass(StringView) const;
	const ProgramData *getProgram(StringView) const;
	const PipelineData *getPipeline(StringView) const;
	const Attachment *getAttachment(StringView) const;

	Vector<Rc<Attachment>> getOutput() const;
	Vector<Rc<Attachment>> getOutput(AttachmentType) const;

	// get next frame order dumber for this queue
	uint64_t incrementOrder();

	// Prepare queue to be used on target device
	bool prepare(gl::Device &);

	void beginFrame(FrameRequest &);
	void endFrame(FrameRequest &);

	bool usesSamplers() const;

protected:
	QueueData *_data = nullptr;
};

class Queue::Builder final {
public:
	Builder(StringView);
	~Builder();

	PassData * addRenderPass(const Rc<Pass> &);

	AttachmentRef *addPassInput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<BufferAttachment> &, AttachmentDependencyInfo);
	AttachmentRef *addPassOutput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<BufferAttachment> &, AttachmentDependencyInfo);

	AttachmentRef *addPassInput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<GenericAttachment> &, AttachmentDependencyInfo);
	AttachmentRef *addPassOutput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<GenericAttachment> &, AttachmentDependencyInfo);

	ImageAttachmentRef *addPassInput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &, AttachmentDependencyInfo);
	ImageAttachmentRef *addPassOutput(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &, AttachmentDependencyInfo);

	Pair<ImageAttachmentRef *, ImageAttachmentRef *> addPassResolve(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &color, const Rc<ImageAttachment> &resolve,
			AttachmentDependencyInfo colorDep, AttachmentDependencyInfo resolveDep);

	ImageAttachmentRef *addPassDepthStencil(const Rc<Pass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &, AttachmentDependencyInfo);

	bool addSubpassDependency(const Rc<Pass> &, uint32_t srcSubpass, PipelineStage srcStage, AccessType srcAccess,
			uint32_t dstSubpass, PipelineStage dstStage, AccessType dstAccess, bool byRegion = true);

	bool addInput(const Rc<Attachment> &, AttachmentOps ops = AttachmentOps::WritesColor | AttachmentOps::WritesStencil);
	bool addOutput(const Rc<Attachment> &, AttachmentOps ops = AttachmentOps::ReadColor | AttachmentOps::ReadStencil);

	// add program, copy all data
	const ProgramData * addProgram(StringView key, SpanView<uint32_t>, const ProgramInfo * = nullptr);

	// add program, take shader data by ref, data should exists for all resource lifetime
	const ProgramData * addProgramByRef(StringView key, SpanView<uint32_t>, const ProgramInfo * = nullptr);

	// add program, data will be acquired with callback when needed
	const ProgramData * addProgram(StringView key, const memory::function<void(const ProgramData::DataCallback &)> &,
			const ProgramInfo * = nullptr);

	template <typename ... Args>
	const PipelineData * addPipeline(const Rc<Pass> &pass, uint32_t subpass, StringView key, Args && ...args) {
		if (auto p = emplacePipeline(pass, subpass, key)) {
			if (setPipelineOptions(*p, std::forward<Args>(args)...)) {
				return p;
			}
			erasePipeline(pass, subpass, p);
		}
		return nullptr;
	}

	// resources, that will be compiled with RenderQueue
	void setInternalResource(Rc<Resource> &&);

	// external resources, that should be compiled when added
	void addLinkedResource(const Rc<Resource> &);

	void setBeginCallback(Function<void(FrameRequest &)> &&);
	void setEndCallback(Function<void(FrameRequest &)> &&);

protected:
	PipelineData *emplacePipeline(const Rc<Pass> &, uint32_t, StringView key);
	void erasePipeline(const Rc<Pass> &, uint32_t, PipelineData *);

	bool setPipelineOption(PipelineData &f, DynamicState);
	bool setPipelineOption(PipelineData &f, const Vector<SpecializationInfo> &);
	bool setPipelineOption(PipelineData &f, const PipelineMaterialInfo &);

	template <typename T>
	bool setPipelineOptions(PipelineData &f, T && t) {
		return setPipelineOption(f, std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	bool setPipelineOptions(PipelineData &f, T && t, Args && ... args) {
		if (!setPipelineOption(f, std::forward<T>(t))) {
			return false;
		}
		return setPipelineOptions(f, std::forward<Args>(args)...);
	}

	memory::pool_t *getPool() const;

	PassData *getPassData(const Rc<Pass> &) const;
	SubpassData *getSubpassData(const Rc<Pass> &, uint32_t) const;

	friend class Queue;

	QueueData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEQUEUE_H_ */
