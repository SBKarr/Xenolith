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

namespace stappler::xenolith::gl {

struct RenderPassData;

class Attachment;
class AttachmentDescriptor;
class AttachmentRef;

class BufferAttachment;
class BufferAttachmentDescriptor;
class BufferAttachmentRef;

class ImageAttachment;
class ImageAttachmentDescriptor;
class ImageAttachmentRef;

class FrameHandle;

class AttachmentHandle;

class RenderPassHandle;

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

	virtual void onSwapchainUpdate(const ImageInfo &) { }

	virtual AttachmentDescriptor *addDescriptor(RenderPassData *);

	virtual bool isCompatible(const ImageInfo &) const { return false; }

	virtual void sortDescriptors(RenderQueue &queue);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameHandle &);

	virtual RenderPassData *getFirstRenderPass() const;
	virtual RenderPassData *getLastRenderPass() const;
	virtual RenderPassData *getNextRenderPass(RenderPassData *) const;
	virtual RenderPassData *getPrevRenderPass(RenderPassData *) const;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) {
		return nullptr;
	}

	String _name;
	bool _transient = false;
	AttachmentType _type = AttachmentType::SwapchainImage;
	AttachmentUsage _usage = AttachmentUsage::None;
	AttachmentOps _ops = AttachmentOps::Undefined;
	DescriptorType _descriptorType = DescriptorType::Unknown;
	Vector<Rc<AttachmentDescriptor>> _descriptors;
};

struct PipelineDescriptor {
	StringView name; // for external descriptors
	Attachment *attachment = nullptr;
	DescriptorType type = DescriptorType::Unknown;
	ProgramStage stages = ProgramStage::None;
	uint32_t count = 1;
	uint32_t maxCount = 1;
};

class AttachmentDescriptor : public NamedRef {
public:
	virtual ~AttachmentDescriptor() { }

	virtual bool init(RenderPassData *, Attachment *);

	virtual void clear();
	virtual void reset();

	uint32_t getIndex() const { return _index; }
	void setIndex(uint32_t idx) { _index = idx; }

	AttachmentOps getOps() const { return _ops; }
	void setOps(AttachmentOps ops) { _ops = ops; }

	bool hasDescriptor() const { return _descriptor.type != DescriptorType::Unknown; }

	virtual StringView getName() const override { return _descriptor.attachment->getName(); }

	RenderPassData *getRenderPass() const { return _renderPass; }
	Attachment *getAttachment() const { return _descriptor.attachment; }
	const Vector<Rc<AttachmentRef>> &getRefs() const { return _refs; }

	virtual AttachmentRef *addRef(uint32_t idx, AttachmentUsage);

	virtual void sortRefs(RenderQueue &);

	const PipelineDescriptor &getDescriptor() const { return _descriptor; }

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage) {
		return nullptr;
	}

	RenderPassData *_renderPass;
	uint32_t _index = maxOf<uint32_t>();
	AttachmentOps _ops = AttachmentOps::Undefined;
	Vector<Rc<AttachmentRef>> _refs;
	PipelineDescriptor _descriptor;
};

class AttachmentRef : public Ref {
public:
	virtual ~AttachmentRef() { }

	virtual bool init(AttachmentDescriptor *, uint32_t, AttachmentUsage);

	uint32_t getSubpass() const { return _subpass; }
	AttachmentDescriptor *getDescriptor() const { return _descriptor; }
	AttachmentUsage getUsage() const { return _usage; }

	AttachmentOps getOps() const { return _ops; }
	void setOps(AttachmentOps ops) { _ops = ops; }

	void addUsage(AttachmentUsage usage) { _usage |= usage; }

	virtual void updateLayout();

protected:
	AttachmentDescriptor *_descriptor = nullptr;
	uint32_t _subpass = 0;
	AttachmentUsage _usage = AttachmentUsage::None;
	AttachmentOps _ops = AttachmentOps::Undefined;
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
	virtual BufferAttachmentRef *addBufferRef(uint32_t idx, AttachmentUsage);

protected:
	virtual Rc<AttachmentRef> makeRef(uint32_t idx, AttachmentUsage) override;
};

class BufferAttachmentRef : public AttachmentRef {
public:
protected:
};


class ImageAttachment : public Attachment {
public:
	virtual ~ImageAttachment() { }

	virtual bool init(StringView, const ImageInfo &, AttachmentLayout init = AttachmentLayout::Ignored,
			AttachmentLayout fin = AttachmentLayout::Ignored, bool clear = false);

	virtual const ImageInfo &getInfo() const { return _info; }
	virtual bool shouldClearOnLoad() const { return _clearOnLoad; }

	virtual AttachmentLayout getInitialLayout() const { return _initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _finalLayout; }

	virtual void onSwapchainUpdate(const ImageInfo &) override;

	virtual ImageAttachmentDescriptor *addImageDescriptor(RenderPassData *);

	virtual bool isCompatible(const ImageInfo &) const override;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;

	ImageInfo _info;
	AttachmentLayout _initialLayout = AttachmentLayout::Ignored;
	AttachmentLayout _finalLayout = AttachmentLayout::Ignored;
	bool _clearOnLoad = false;
};

class ImageAttachmentDescriptor : public AttachmentDescriptor {
public:
	virtual bool init(RenderPassData *, ImageAttachment *);

	virtual const ImageInfo &getInfo() const { return ((ImageAttachment *)getAttachment())->getInfo(); }

	virtual AttachmentLoadOp getLoadOp() const { return _loadOp; }
	virtual void setLoadOp(AttachmentLoadOp op) { _loadOp = op; }

	virtual AttachmentLoadOp getStencilLoadOp() const { return _stencilLoadOp; }
	virtual void setStencilLoadOp(AttachmentLoadOp op) { _stencilLoadOp = op; }

	virtual AttachmentStoreOp getStoreOp() const { return _storeOp; }
	virtual void setStoreOp(AttachmentStoreOp op) { _storeOp = op; }

	virtual AttachmentStoreOp getStencilStoreOp() const { return _stencilStoreOp; }
	virtual void setStencilStoreOp(AttachmentStoreOp op) { _stencilStoreOp = op; }

	virtual ImageAttachmentRef *addImageRef(uint32_t idx, AttachmentUsage, AttachmentLayout);

	virtual AttachmentLayout getInitialLayout() const { return _initialLayout; }
	virtual void setInitialLayout(AttachmentLayout l) { _initialLayout = l; }

	virtual AttachmentLayout getFinalLayout() const { return _finalLayout; }
	virtual void setFinalLayout(AttachmentLayout l) { _finalLayout = l; }

protected:
	virtual Rc<ImageAttachmentRef> makeImageRef(uint32_t idx, AttachmentUsage, AttachmentLayout);

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
};

class ImageAttachmentRef : public AttachmentRef {
public:
	virtual bool init(ImageAttachmentDescriptor *, uint32_t, AttachmentUsage, AttachmentLayout);

	virtual const ImageInfo &getInfo() const { return ((ImageAttachmentDescriptor *)_descriptor)->getInfo(); }

	virtual AttachmentLayout getLayout() const { return _layout; }
	virtual void setLayout(AttachmentLayout);

	virtual void updateLayout() override;

protected:
	AttachmentLayout _layout = AttachmentLayout::Undefined;
};


class SwapchainAttachment : public ImageAttachment {
public:
	virtual ~SwapchainAttachment();

	virtual bool init(StringView, const ImageInfo &, AttachmentLayout init = AttachmentLayout::Ignored,
			AttachmentLayout fin = AttachmentLayout::Ignored, bool clear = false);

	const Rc<gl::FrameHandle> &getOwner() const { return _owner; }
	bool acquireForFrame(gl::FrameHandle &);
	bool releaseForFrame(gl::FrameHandle &);

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;

	Rc<gl::FrameHandle> _owner;
	Rc<gl::FrameHandle> _next;
};

class SwapchainAttachmentDescriptor : public ImageAttachmentDescriptor {
public:
	virtual ~SwapchainAttachmentDescriptor() { }

protected:
};

class AttachmentHandle : public Ref {
public:
	virtual ~AttachmentHandle() { }

	virtual bool init(const Rc<Attachment> &, const FrameHandle &);

	virtual bool isAvailable(const FrameHandle &) const { return true; }

	// returns true for immediate setup, false if seyup job was scheduled
	virtual bool setup(FrameHandle &);

	virtual bool isReady() const { return _ready; }
	virtual void setReady(bool);

	virtual bool isInput() const;
	virtual const Rc<Attachment> &getAttachment() const { return _attachment; }

	virtual bool submitInput(FrameHandle &, Rc<AttachmentInputData> &&) {
		return false;
	}

	virtual uint32_t getDescriptorArraySize(const RenderPassHandle &, const PipelineDescriptor &, bool isExternal) const;
	virtual bool isDescriptorDirty(const RenderPassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;

protected:
	bool _ready = false;
	Rc<Attachment> _attachment;
};


}

#endif /* XENOLITH_GL_COMMON_XLGLRENDERATTACHMENT_H_ */
