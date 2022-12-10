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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEATTACHMENT_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEATTACHMENT_H_

#include "XLRenderQueue.h"
#include "XLRenderQueueResource.h"

namespace stappler::xenolith::renderqueue {

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
};

class Attachment : public NamedRef {
public:
	using FrameQueue = renderqueue::FrameQueue;
	using RenderQueue = renderqueue::Queue;
	using PassData = renderqueue::PassData;
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using AttachmentDescriptor = renderqueue::AttachmentDescriptor;

	virtual ~Attachment() { }

	virtual bool init(StringView, AttachmentType);
	virtual void clear();

	virtual void addUsage(AttachmentUsage, AttachmentOps = AttachmentOps::Undefined);

	AttachmentType getType() const { return _type; }
	AttachmentUsage getUsage() const { return _usage; }
	AttachmentOps getOps() const { return _ops; }
	virtual StringView getName() const override { return _name; }
	const Vector<Rc<AttachmentDescriptor>> &getDescriptors() const { return _descriptors; }

	DescriptorType getDescriptorType() const { return _descriptorType; }
	void setDescriptorType(DescriptorType type) { _descriptorType = type; }

	bool isTransient() const { return _transient; }
	void setTransient(bool value) { _transient = value; }

	uint32_t getIndex() const { return _index; }
	void setIndex(uint32_t idx) { _index = idx; }

	// Set callback for frame to acquire input for this
	void setInputCallback(Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> &&);

	// Run input callback for frame and handle
	void acquireInput(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&);

	virtual bool validateInput(const Rc<AttachmentInputData> &) const;

	virtual AttachmentDescriptor *addDescriptor(PassData *);

	virtual bool isCompatible(const gl::ImageInfo &) const { return false; }

	virtual void sortDescriptors(Queue &queue, gl::Device &dev);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &);

	virtual Vector<PassData *> getRenderPasses() const;
	virtual PassData *getFirstRenderPass() const;
	virtual PassData *getLastRenderPass() const;
	virtual PassData *getNextRenderPass(PassData *) const;
	virtual PassData *getPrevRenderPass(PassData *) const;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(PassData *) {
		return nullptr;
	}

	uint32_t _index = 0;
	String _name;
	bool _transient = false;
	AttachmentType _type = AttachmentType::Image;
	AttachmentUsage _usage = AttachmentUsage::None;
	AttachmentOps _ops = AttachmentOps::Undefined;
	DescriptorType _descriptorType = DescriptorType::Unknown;
	Vector<Rc<AttachmentDescriptor>> _descriptors;

	Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> _inputCallback;
};

class AttachmentDescriptor : public NamedRef {
public:
	using PassData = renderqueue::PassData;
	using Attachment = renderqueue::Attachment;

	virtual ~AttachmentDescriptor() { }

	virtual bool init(PassData *, Attachment *);

	virtual void clear();
	virtual void reset();

	uint32_t getIndex() const { return _index; }
	void setIndex(uint32_t idx);

	AttachmentOps getOps() const { return _ops; }
	void setOps(AttachmentOps ops) { _ops = ops; }

	FrameRenderPassState getRequiredRenderPassState() const { return _dependency.requiredRenderPassState; }

	const AttachmentDependencyInfo &getDependency() const { return _dependency; }

	bool hasDescriptor() const { return _descriptor.type != DescriptorType::Unknown; }

	virtual StringView getName() const override { return _descriptor.attachment->getName(); }

	PassData *getRenderPass() const { return _renderPass; }
	Attachment *getAttachment() const { return _descriptor.attachment; }
	const Vector<Rc<AttachmentRef>> &getRefs() const { return _refs; }
	bool usesTextureSet() const { return _usesTextureSet; }

	virtual AttachmentRef *addRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo);

	virtual void sortRefs(Queue &, gl::Device &dev);

	const PipelineDescriptor &getDescriptor() const { return _descriptor; }

	DescriptorType getDescriptorType() const { return _descriptor.type; }
	void setDescriptorType(DescriptorType type) { _descriptor.type = type; }

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo) {
		return nullptr;
	}

	PassData *_renderPass;
	uint32_t _index = maxOf<uint32_t>();
	AttachmentOps _ops = AttachmentOps::Undefined;
	Vector<Rc<AttachmentRef>> _refs;
	PipelineDescriptor _descriptor;
	AttachmentDependencyInfo _dependency;
	bool _usesTextureSet = false;
};

class AttachmentRef : public Ref {
public:
	virtual ~AttachmentRef() { }

	virtual bool init(AttachmentDescriptor *, uint32_t, AttachmentUsage, AttachmentDependencyInfo);

	uint32_t getSubpass() const { return _subpass; }
	AttachmentDescriptor *getDescriptor() const { return _descriptor; }
	Attachment *getAttachment() const { return _descriptor->getAttachment(); }
	AttachmentUsage getUsage() const { return _usage; }
	const AttachmentDependencyInfo &getDependency() const { return _dependency; }

	AttachmentOps getOps() const { return _ops; }
	void setOps(AttachmentOps ops) { _ops = ops; }

	void addUsage(AttachmentUsage usage) { _usage |= usage; }

	// try to merge dependencies for various usage
	void addDependency(AttachmentDependencyInfo);

	virtual void updateLayout();

protected:
	AttachmentDescriptor *_descriptor = nullptr;
	uint32_t _subpass = 0;
	AttachmentUsage _usage = AttachmentUsage::None;
	AttachmentOps _ops = AttachmentOps::Undefined;
	AttachmentDependencyInfo _dependency;
};


class BufferAttachment : public Attachment {
public:
	virtual ~BufferAttachment() { }

	virtual bool init(StringView, const gl::BufferInfo &);
	virtual void clear() override;

	const gl::BufferInfo &getInfo() const { return _info; }

	virtual BufferAttachmentDescriptor *addBufferDescriptor(PassData *);

protected:
	using Attachment::init;

	virtual Rc<AttachmentDescriptor> makeDescriptor(PassData *) override;

	gl::BufferInfo _info;
};

class BufferAttachmentDescriptor : public AttachmentDescriptor {
public:
	virtual ~BufferAttachmentDescriptor() { }

	virtual BufferAttachmentRef *addBufferRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo);

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo) override;
};

class BufferAttachmentRef : public AttachmentRef {
public:
protected:
};

class ImageAttachment : public Attachment {
public:
	virtual ~ImageAttachment() { }

	struct AttachmentInfo {
		AttachmentLayout initialLayout = AttachmentLayout::Ignored;
		AttachmentLayout finalLayout = AttachmentLayout::Ignored;
		bool clearOnLoad = false;
		Color4F clearColor = Color4F::BLACK;
		Function<Extent3(const FrameQueue &)> frameSizeCallback;
		ColorMode colorMode;
	};

	virtual bool init(StringView, const gl::ImageInfo &, AttachmentInfo &&);

	virtual const gl::ImageInfo &getImageInfo() const { return _imageInfo; }
	virtual gl::ImageInfo getAttachmentInfo(const AttachmentHandle *, Extent3 e) const { return _imageInfo; }
	virtual bool shouldClearOnLoad() const { return _attachmentInfo.clearOnLoad; }
	virtual bool isFrameBasedSize() const { return _attachmentInfo.frameSizeCallback != nullptr; }
	virtual Color4F getClearColor() const { return _attachmentInfo.clearColor; }
	virtual ColorMode getColorMode() const { return _attachmentInfo.colorMode; }

	virtual AttachmentLayout getInitialLayout() const { return _attachmentInfo.initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _attachmentInfo.finalLayout; }

	virtual void addImageUsage(gl::ImageUsage);

	virtual ImageAttachmentDescriptor *addImageDescriptor(PassData *, DescriptorType = DescriptorType::Unknown);

	virtual bool isCompatible(const gl::ImageInfo &) const override;

	virtual Extent3 getSizeForFrame(const FrameQueue &) const;

protected:
	using Attachment::init;

	virtual Rc<AttachmentDescriptor> makeDescriptor(PassData *) override;

	gl::ImageInfo _imageInfo;
	AttachmentInfo _attachmentInfo;
};

class ImageAttachmentDescriptor : public AttachmentDescriptor {
public:
	virtual ~ImageAttachmentDescriptor() { }

	virtual bool init(PassData *, ImageAttachment *);

	virtual const gl::ImageInfo &getImageInfo() const { return ((ImageAttachment *)getAttachment())->getImageInfo(); }

	AttachmentLoadOp getLoadOp() const { return _loadOp; }
	void setLoadOp(AttachmentLoadOp op) { _loadOp = op; }

	AttachmentLoadOp getStencilLoadOp() const { return _stencilLoadOp; }
	void setStencilLoadOp(AttachmentLoadOp op) { _stencilLoadOp = op; }

	AttachmentStoreOp getStoreOp() const { return _storeOp; }
	void setStoreOp(AttachmentStoreOp op) { _storeOp = op; }

	AttachmentStoreOp getStencilStoreOp() const { return _stencilStoreOp; }
	void setStencilStoreOp(AttachmentStoreOp op) { _stencilStoreOp = op; }

	virtual ImageAttachmentRef *addImageRef(uint32_t idx, AttachmentUsage, AttachmentLayout, AttachmentDependencyInfo);

	AttachmentLayout getInitialLayout() const { return _initialLayout; }
	void setInitialLayout(AttachmentLayout l) { _initialLayout = l; }

	AttachmentLayout getFinalLayout() const { return _finalLayout; }
	void setFinalLayout(AttachmentLayout l) { _finalLayout = l; }

	ColorMode getColorMode() const { return _colorMode; }
	void setColorMode(ColorMode value) { _colorMode = value; }

	ImageAttachment *getImageAttachment() const;

protected:
	using AttachmentDescriptor::init;

	virtual Rc<ImageAttachmentRef> makeImageRef(uint32_t idx, AttachmentUsage, AttachmentLayout, AttachmentDependencyInfo);

	// calculated initial layout
	// for first descriptor in execution chain - initial layout of queue's attachment or first usage layout
	// for others - final layout of previous descriptor in chain of execution
	AttachmentLayout _initialLayout = AttachmentLayout::Undefined;

	// calculated final layout
	// for last descriptor in execution chain - final layout of queue's attachment or last usage layout
	// for others - last usage layout
	AttachmentLayout _finalLayout = AttachmentLayout::Undefined;

	AttachmentLoadOp _loadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp _storeOp = AttachmentStoreOp::DontCare;
	AttachmentLoadOp _stencilLoadOp = AttachmentLoadOp::DontCare;
	AttachmentStoreOp _stencilStoreOp = AttachmentStoreOp::DontCare;

	ColorMode _colorMode;
};

class ImageAttachmentRef : public AttachmentRef {
public:
	virtual ~ImageAttachmentRef() { }

	virtual bool init(ImageAttachmentDescriptor *, uint32_t, AttachmentUsage, AttachmentLayout, AttachmentDependencyInfo);

	virtual const gl::ImageInfo &getImageInfo() const { return ((ImageAttachmentDescriptor *)_descriptor)->getImageInfo(); }

	virtual AttachmentLayout getLayout() const { return _layout; }
	virtual void setLayout(AttachmentLayout);

	virtual void updateLayout() override;

protected:
	using AttachmentRef::init;

	AttachmentLayout _layout = AttachmentLayout::Undefined;
};

class GenericAttachment : public Attachment {
public:
	virtual ~GenericAttachment() { }

	virtual bool init(StringView);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using Attachment::init;

	virtual Rc<AttachmentDescriptor> makeDescriptor(PassData *) override;
};

class GenericAttachmentDescriptor : public AttachmentDescriptor {
public:
	virtual ~GenericAttachmentDescriptor() { }

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo) override;
};

class GenericAttachmentRef : public AttachmentRef {
public:
	virtual ~GenericAttachmentRef() { }

protected:
};

class AttachmentHandle : public Ref {
public:
	using PassHandle = renderqueue::PassHandle;
	using FrameQueue = renderqueue::FrameQueue;
	using FrameHandle = renderqueue::FrameHandle;
	using PipelineDescriptor = renderqueue::PipelineDescriptor;
	using Attachment = renderqueue::Attachment;

	virtual ~AttachmentHandle() { }

	virtual bool init(const Rc<Attachment> &, const FrameQueue &);
	virtual void setQueueData(FrameAttachmentData &);
	virtual FrameAttachmentData *getQueueData() const { return _queueData; }

	virtual bool isAvailable(const FrameQueue &) const { return true; }

	// returns true for immediate setup, false if setup job was scheduled
	virtual bool setup(FrameQueue &, Function<void(bool)> &&);

	virtual void finalize(FrameQueue &, bool successful);

	virtual bool isInput() const;
	virtual bool isOutput() const;
	virtual const Rc<Attachment> &getAttachment() const { return _attachment; }
	StringView getName() const { return _attachment->getName(); }

	virtual void submitInput(FrameQueue &, Rc<AttachmentInputData> &&, Function<void(bool)> &&cb);

	virtual uint32_t getDescriptorArraySize(const PassHandle &, const PipelineDescriptor &, bool isExternal) const;
	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;

protected:
	Rc<Attachment> _attachment;
	FrameAttachmentData *_queueData = nullptr;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEATTACHMENT_H_ */
