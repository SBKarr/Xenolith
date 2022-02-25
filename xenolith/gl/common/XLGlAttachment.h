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

#ifndef XENOLITH_GL_COMMON_XLGLRENDERATTACHMENT_H_
#define XENOLITH_GL_COMMON_XLGLRENDERATTACHMENT_H_

#include "XLGlObject.h"
#include "XLGlResource.h"
#include "XLGlFrameQueue.h"

namespace stappler::xenolith::gl {

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

	virtual AttachmentDescriptor *addDescriptor(RenderPassData *);

	virtual bool isCompatible(const ImageInfo &) const { return false; }

	virtual void sortDescriptors(RenderQueue &queue, Device &dev);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &);

	virtual Vector<RenderPassData *> getRenderPasses() const;
	virtual RenderPassData *getFirstRenderPass() const;
	virtual RenderPassData *getLastRenderPass() const;
	virtual RenderPassData *getNextRenderPass(RenderPassData *) const;
	virtual RenderPassData *getPrevRenderPass(RenderPassData *) const;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) {
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

class AttachmentDescriptor : public NamedRef {
public:
	virtual ~AttachmentDescriptor() { }

	virtual bool init(RenderPassData *, Attachment *);

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

	RenderPassData *getRenderPass() const { return _renderPass; }
	Attachment *getAttachment() const { return _descriptor.attachment; }
	const Vector<Rc<AttachmentRef>> &getRefs() const { return _refs; }
	bool usesTextureSet() const { return _usesTextureSet; }

	virtual AttachmentRef *addRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo);

	virtual void sortRefs(RenderQueue &, Device &dev);

	const PipelineDescriptor &getDescriptor() const { return _descriptor; }

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo) {
		return nullptr;
	}

	RenderPassData *_renderPass;
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

	virtual bool init(StringView, const BufferInfo &);
	virtual void clear();

	const BufferInfo &getInfo() const { return _info; }

	virtual BufferAttachmentDescriptor *addBufferDescriptor(RenderPassData *);

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;

	BufferInfo _info;
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

struct ImageAttachmentObject final : public Ref {
	virtual ~ImageAttachmentObject() { }

	void rearmSemaphores(Device &);

	Extent3 extent;
	Rc<ImageObject> image;
	Rc<Semaphore> waitSem;
	Rc<Semaphore> signalSem;
	HashMap<const RenderPassData *, Rc<ImageView>> views;
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

	virtual bool init(StringView, const ImageInfo &, AttachmentInfo &&);

	virtual const ImageInfo &getInfo() const { return _imageInfo; }
	virtual bool shouldClearOnLoad() const { return _attachmentInfo.clearOnLoad; }
	virtual bool isFrameBasedSize() const { return _attachmentInfo.frameSizeCallback != nullptr; }
	virtual Color4F getClearColor() const { return _attachmentInfo.clearColor; }
	virtual ColorMode getColorMode() const { return _attachmentInfo.colorMode; }

	virtual AttachmentLayout getInitialLayout() const { return _attachmentInfo.initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _attachmentInfo.finalLayout; }

	virtual void addImageUsage(gl::ImageUsage);

	virtual ImageAttachmentDescriptor *addImageDescriptor(RenderPassData *);

	virtual bool isCompatible(const ImageInfo &) const override;

	virtual Extent3 getSizeForFrame(const FrameQueue &) const;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;

	ImageInfo _imageInfo;
	AttachmentInfo _attachmentInfo;
};

class ImageAttachmentDescriptor : public AttachmentDescriptor {
public:
	virtual bool init(RenderPassData *, ImageAttachment *);

	virtual const ImageInfo &getInfo() const { return ((ImageAttachment *)getAttachment())->getInfo(); }

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
	virtual bool init(ImageAttachmentDescriptor *, uint32_t, AttachmentUsage, AttachmentLayout, AttachmentDependencyInfo);

	virtual const ImageInfo &getInfo() const { return ((ImageAttachmentDescriptor *)_descriptor)->getInfo(); }

	virtual AttachmentLayout getLayout() const { return _layout; }
	virtual void setLayout(AttachmentLayout);

	virtual void updateLayout() override;

protected:
	AttachmentLayout _layout = AttachmentLayout::Undefined;
};

class GenericAttachment : public Attachment {
public:
	virtual bool init(StringView);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;
};

class GenericAttachmentDescriptor : public AttachmentDescriptor {
public:
protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage, AttachmentDependencyInfo) override;
};

class GenericAttachmentRef : public AttachmentRef {
public:
protected:
};

class AttachmentHandle : public Ref {
public:
	virtual ~AttachmentHandle() { }

	virtual bool init(const Rc<Attachment> &, const FrameQueue &);
	virtual void setQueueData(FrameQueueAttachmentData &);

	virtual bool isAvailable(const FrameQueue &) const { return true; }

	// returns true for immediate setup, false if setup job was scheduled
	virtual bool setup(FrameQueue &, Function<void(bool)> &&);

	virtual void finalize(FrameQueue &, bool successful);

	virtual bool isInput() const;
	virtual bool isOutput() const;
	virtual const Rc<Attachment> &getAttachment() const { return _attachment; }

	virtual void submitInput(FrameQueue &, Rc<AttachmentInputData> &&, Function<void(bool)> &&cb) { cb(true); }

	virtual uint32_t getDescriptorArraySize(const RenderPassHandle &, const PipelineDescriptor &, bool isExternal) const;
	virtual bool isDescriptorDirty(const RenderPassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;

protected:
	Rc<Attachment> _attachment;
	FrameQueueAttachmentData *_queueData = nullptr;
};


}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERATTACHMENT_H_ */
