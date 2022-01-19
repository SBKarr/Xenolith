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

#ifndef XENOLITH_GL_COMMON_XLGLRENDERQUEUE_H_
#define XENOLITH_GL_COMMON_XLGLRENDERQUEUE_H_

#include "XLGlAttachment.h"
#include "XLGlRenderPass.h"

namespace stappler::xenolith::gl {

struct RenderPassData;
class Swapchain;

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

struct RenderSubpassDependency {
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

inline bool operator < (const RenderSubpassDependency &l, const RenderSubpassDependency &r) { return l.value() < r.value(); }
inline bool operator == (const RenderSubpassDependency &l, const RenderSubpassDependency &r) { return l.value() == r.value(); }
inline bool operator != (const RenderSubpassDependency &l, const RenderSubpassDependency &r) { return l.value() != r.value(); }

struct RenderSubpassData {
	RenderSubpassData() = default;
	RenderSubpassData(const RenderSubpassData &) = default;
	RenderSubpassData(RenderSubpassData &&) = default;

	RenderSubpassData &operator=(const RenderSubpassData &) = delete;
	RenderSubpassData &operator=(RenderSubpassData &&) = delete;

	uint32_t index = 0;
	RenderPassData *renderPass = nullptr;

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

struct RenderPassData : NamedMem {
	RenderPassData() = default;
	RenderPassData(const RenderPassData &) = default;
	RenderPassData(RenderPassData &&) = default;

	RenderPassData &operator=(const RenderPassData &) = delete;
	RenderPassData &operator=(RenderPassData &&) = delete;

	memory::vector<AttachmentDescriptor *> descriptors;
	memory::vector<RenderSubpassData> subpasses;
	memory::vector<RenderSubpassDependency> dependencies;
	memory::vector<const PipelineDescriptor *> queueDescriptors;
	memory::vector<PipelineDescriptor> extraDescriptors;

	RenderOrdering ordering = RenderOrderingLowest;
	bool isPresentable = false;
	bool usesSamplers = false;
	bool hasUpdateAfterBind = false;

	Rc<RenderPass> renderPass;
	Rc<RenderPassImpl> impl;
	memory::vector<Rc<Framebuffer>> framebuffers;
};

class RenderQueue : public NamedRef {
public:
	class Builder;
	struct QueueData;

	enum Mode {
		Continuous, // spawn new frame when engine ready
		RenderOnDemand, // spawn new frame when application requests new frame
	};

	RenderQueue();
	virtual ~RenderQueue();

	virtual bool init(Builder &&);

	bool isCompiled() const;
	void setCompiled(bool);

	bool updateSwapchainInfo(const ImageInfo &);
	const ImageInfo *getSwapchainImageInfo() const;

	bool isPresentable() const;
	bool isCompatible(const ImageInfo &) const;

	virtual StringView getName() const override;

	const HashTable<ProgramData *> &getPrograms() const;
	const HashTable<RenderPassData *> &getPasses() const;
	const HashTable<PipelineData *> &getPipelines() const;
	const HashTable<Rc<Attachment>> &getAttachments() const;
	const HashTable<Rc<Resource>> &getLinkedResources() const;
	Rc<Resource> getInternalResource() const;

	const memory::vector<Attachment *> &getInputAttachments() const;
	const memory::vector<Attachment *> &getOutputAttachments() const;

	const RenderPassData *getPass(StringView) const;
	const ProgramData *getProgram(StringView) const;
	const PipelineData *getPipeline(StringView) const;
	const Attachment *getAttachment(StringView) const;

	Vector<Rc<Attachment>> getOutput() const;
	Vector<Rc<Attachment>> getOutput(AttachmentType) const;

	// get next frame order dumber for this queue
	uint64_t incrementOrder();

	// Prepare queue to be used on target device
	bool prepare(Device &);

	void beginFrame(gl::FrameHandle &);
	void endFrame(gl::FrameHandle &);

	void enable(const Swapchain *);
	void disable();

	bool usesSamplers() const;

protected:
	QueueData *_data = nullptr;
};

class RenderQueue::Builder final {
public:
	Builder(StringView, Mode);
	~Builder();

	void setMode(Mode);

	RenderPassData * addRenderPass(const Rc<RenderPass> &);

	AttachmentRef *addPassInput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<BufferAttachment> &);
	AttachmentRef *addPassOutput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<BufferAttachment> &);

	AttachmentRef *addPassInput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<GenericAttachment> &);
	AttachmentRef *addPassOutput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<GenericAttachment> &);

	ImageAttachmentRef *addPassInput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);
	ImageAttachmentRef *addPassOutput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);
	Pair<ImageAttachmentRef *, ImageAttachmentRef *> addPassResolve(const Rc<RenderPass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &color, const Rc<ImageAttachment> &resolve);

	ImageAttachmentRef *addPassDepthStencil(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);

	bool addSubpassDependency(const Rc<RenderPass> &, uint32_t srcSubpass, PipelineStage srcStage, AccessType srcAccess,
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
	const PipelineData * addPipeline(const Rc<RenderPass> &pass, uint32_t subpass, StringView key, Args && ...args) {
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

	void setBeginCallback(Function<void(gl::FrameHandle &)> &&);
	void setEndCallback(Function<void(gl::FrameHandle &)> &&);

	void setEnableCallback(Function<void(const Swapchain *)> &&);
	void setDisableCallback(Function<void()> &&);

protected:
	PipelineData *emplacePipeline(const Rc<RenderPass> &, uint32_t, StringView key);
	void erasePipeline(const Rc<RenderPass> &, uint32_t, PipelineData *);

	bool setPipelineOption(PipelineData &f, DynamicState);
	bool setPipelineOption(PipelineData &f, const Vector<SpecializationInfo> &);

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

	RenderPassData *getPassData(const Rc<RenderPass> &) const;
	RenderSubpassData *getSubpassData(const Rc<RenderPass> &, uint32_t) const;

	friend class RenderQueue;

	QueueData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERQUEUE_H_ */
