/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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
	memory::vector<BufferAttachmentRef *> inputBuffers;
	memory::vector<BufferAttachmentRef *> outputBuffers;

	memory::vector<ImageAttachmentRef *> inputImages;
	memory::vector<ImageAttachmentRef *> outputImages;
	memory::vector<ImageAttachmentRef *> resolveImages;
	ImageAttachmentRef *depthStencil;
	memory::vector<uint32_t> preserve;
};

struct RenderPassData : NamedMem {
	HashTable<PipelineData *> pipelines;
	memory::vector<AttachmentDescriptor *> descriptors;
	memory::vector<RenderSubpassData> subpasses;
	memory::vector<RenderSubpassDependency> dependencies;

	RenderOrdering ordering = RenderOrderingLowest;
	bool isPresentable = false;

	Rc<RenderPass> renderPass;
	Rc<RenderPassImpl> impl;
	memory::vector<Rc<Framebuffer>> framebuffers;
};

struct PipelineDescriptor {
	Attachment *attachment = nullptr;
	DescriptorType type = DescriptorType::Sampler;
	ProgramStage stages = ProgramStage::None;
};

struct PipelineLayoutData {
	memory::vector<PipelineDescriptor> queueDescriptors;
	memory::vector<PipelineDescriptor> extraDescriptors;
	size_t textureSwapSize = 0;

	Rc<PipelineLayout> layout;
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

	virtual void setCompiled(bool);
	virtual bool updateSwapchainInfo(const ImageInfo &);

	bool isPresentable() const;
	bool isCompatible(const ImageInfo &) const;

	virtual StringView getName() const override;

	const HashTable<ProgramData *> &getPrograms() const;
	const HashTable<RenderPassData *> &getPasses() const;
	const HashTable<Rc<Attachment>> &getAttachments() const;
	const HashTable<Rc<Resource>> &getResources() const;

	const ProgramData *getProgram(StringView) const;

	const PipelineLayoutData &getPipelineLayout() const;
	void setPipelineLayout(Rc<PipelineLayout> &&);

	Vector<Rc<Attachment>> getOutput() const;
	Vector<Rc<Attachment>> getOutput(AttachmentType) const;

	bool prepare();

protected:
	QueueData *_data = nullptr;
};

class RenderQueue::Builder final {
public:
	Builder(StringView, Mode);
	~Builder();

	void setMode(Mode);

	void addRenderPass(const Rc<RenderPass> &);

	AttachmentRef *addPassInput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<BufferAttachment> &);
	AttachmentRef *addPassOutput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<BufferAttachment> &);

	ImageAttachmentRef *addPassInput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);
	ImageAttachmentRef *addPassOutput(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);
	Pair<ImageAttachmentRef *, ImageAttachmentRef *> addPassResolve(const Rc<RenderPass> &, uint32_t subpassIdx,
			const Rc<ImageAttachment> &color, const Rc<ImageAttachment> &resolve);

	ImageAttachmentRef *addPassDepthStencil(const Rc<RenderPass> &, uint32_t subpassIdx, const Rc<ImageAttachment> &);

	bool addSubpassDependency(const Rc<RenderPass> &, uint32_t srcSubpass, PipelineStage srcStage, AccessType srcAccess,
			uint32_t dstSubpass, PipelineStage dstStage, AccessType dstAccess, bool byRegion = true);

	bool addInput(const Rc<Attachment> &, AttachmentOps ops = AttachmentOps::WritesColor | AttachmentOps::WritesStencil);
	bool addOutput(const Rc<Attachment> &, AttachmentOps ops = AttachmentOps::ReadColor | AttachmentOps::ReadStencil);

	bool addResource(const Rc<Resource> &);

	// add program, copy all data
	const ProgramData * addProgram(StringView key, ProgramStage, SpanView<uint32_t>);

	// add program, take shader data by ref, data should exists for all resource lifetime
	const ProgramData * addProgramByRef(StringView key, ProgramStage, SpanView<uint32_t>);

	// add program, data will be acquired with callback when needed
	const ProgramData * addProgram(StringView key, ProgramStage, const memory::function<void(const ProgramData::DataCallback &)> &);

	template <typename ... Args>
	const PipelineData * addPipeline(const Rc<RenderPass> &pass, StringView key, Args && ...args) {
		if (auto p = emplacePipeline(pass, key)) {
			if (setPipelineOptions(*p, std::forward<Args>(args)...)) {
				return p;
			}
			erasePipeline(pass, p);
		}
		return nullptr;
	}

protected:
	PipelineData *emplacePipeline(const Rc<RenderPass> &, StringView key);
	void erasePipeline(const Rc<RenderPass> &, PipelineData *);

	bool setPipelineOption(PipelineData &f, VertexFormat);
	bool setPipelineOption(PipelineData &f, LayoutFormat);
	bool setPipelineOption(PipelineData &f, DynamicState);
	bool setPipelineOption(PipelineData &f, const Vector<const ProgramData *> &);

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

	friend class RenderQueue;

	QueueData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERQUEUE_H_ */
