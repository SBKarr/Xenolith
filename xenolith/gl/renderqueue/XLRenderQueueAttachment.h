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

class Attachment : public NamedRef {
public:
	using FrameQueue = renderqueue::FrameQueue;
	using RenderQueue = renderqueue::Queue;
	using PassData = renderqueue::PassData;
	using AttachmentData = renderqueue::AttachmentData;
	using AttachmentHandle = renderqueue::AttachmentHandle;
	using AttachmentBuilder = renderqueue::AttachmentBuilder;

	virtual ~Attachment() { }

	virtual bool init(AttachmentBuilder &builder);
	virtual void clear();

	virtual StringView getName() const override;
	AttachmentUsage getUsage() const;
	bool isTransient() const;

	// Set callback for frame to acquire input for this
	void setInputCallback(Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> &&);

	// Run input callback for frame and handle
	void acquireInput(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&);

	virtual bool validateInput(const Rc<AttachmentInputData> &) const;

	virtual bool isCompatible(const gl::ImageInfo &) const { return false; }

	virtual void sortDescriptors(Queue &queue, gl::Device &dev);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &);

	virtual Vector<const PassData *> getRenderPasses() const;
	virtual const PassData *getFirstRenderPass() const;
	virtual const PassData *getLastRenderPass() const;
	virtual const PassData *getNextRenderPass(const PassData *) const;
	virtual const PassData *getPrevRenderPass(const PassData *) const;

	const AttachmentData *getData() const { return _data; }

protected:
	const AttachmentData *_data = nullptr;

	Function<void(FrameQueue &, const Rc<AttachmentHandle> &, Function<void(bool)> &&)> _inputCallback;
};

class BufferAttachment : public Attachment {
public:
	virtual ~BufferAttachment() { }

	virtual bool init(AttachmentBuilder &builder, const gl::BufferInfo &);
	virtual void clear() override;

	const gl::BufferInfo &getInfo() const { return _info; }

protected:
	using Attachment::init;

	gl::BufferInfo _info;
};

class ImageAttachment : public Attachment {
public:
	virtual ~ImageAttachment() { }

	struct AttachmentInfo {
		AttachmentLayout initialLayout = AttachmentLayout::Ignored;
		AttachmentLayout finalLayout = AttachmentLayout::Ignored;
		bool clearOnLoad = false;
		Color4F clearColor = Color4F::BLACK;
		Function<Extent3(const FrameQueue &, const gl::ImageInfoData *specialization)> frameSizeCallback;
		ColorMode colorMode;
	};

	virtual bool init(AttachmentBuilder &builder, const gl::ImageInfo &, AttachmentInfo &&);

	virtual const gl::ImageInfo &getImageInfo() const { return _imageInfo; }
	virtual gl::ImageInfo getAttachmentInfo(const AttachmentHandle *, Extent3 e) const { return _imageInfo; }
	virtual bool shouldClearOnLoad() const { return _attachmentInfo.clearOnLoad; }
	virtual bool isFrameBasedSize() const { return _attachmentInfo.frameSizeCallback != nullptr; }
	virtual Color4F getClearColor() const { return _attachmentInfo.clearColor; }
	virtual ColorMode getColorMode() const { return _attachmentInfo.colorMode; }

	virtual AttachmentLayout getInitialLayout() const { return _attachmentInfo.initialLayout; }
	virtual AttachmentLayout getFinalLayout() const { return _attachmentInfo.finalLayout; }

	virtual void addImageUsage(gl::ImageUsage);

	virtual bool isCompatible(const gl::ImageInfo &) const override;

	virtual Extent3 getSizeForFrame(const FrameQueue &) const;

	virtual gl::ImageViewInfo getImageViewInfo(const gl::ImageInfoData &info, const AttachmentPassData &) const;
	virtual Vector<gl::ImageViewInfo> getImageViews(const gl::ImageInfoData &info) const;

protected:
	using Attachment::init;

	gl::ImageInfo _imageInfo;
	AttachmentInfo _attachmentInfo;
};

class GenericAttachment : public Attachment {
public:
	virtual ~GenericAttachment() { }

	virtual bool init(AttachmentBuilder &builder);

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using Attachment::init;
};

class AttachmentHandle : public Ref {
public:
	using PassHandle = renderqueue::PassHandle;
	using FrameQueue = renderqueue::FrameQueue;
	using FrameHandle = renderqueue::FrameHandle;
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
