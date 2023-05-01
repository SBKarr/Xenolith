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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKATTACHMENT_H_

#include "XLVkFramebuffer.h"
#include "XLVkSync.h"
#include "XLRenderQueueAttachment.h"

namespace stappler::xenolith::vk {

class DeviceBuffer;
class QueuePassHandle;

class BufferAttachment : public renderqueue::BufferAttachment {
public:
	virtual ~BufferAttachment() { }

protected:
};

class ImageAttachment : public renderqueue::ImageAttachment {
public:
	virtual ~ImageAttachment() { }

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};


class BufferAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~BufferAttachmentHandle() { }

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) { return false; }
};

class ImageAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~ImageAttachmentHandle() { }

	const Rc<renderqueue::ImageStorage> &getImage() const { return _queueData->image; }

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorImageInfo &);
	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &, uint32_t, bool isExternal) const;
};

class TexelAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~TexelAttachmentHandle();

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferViewInfo &) { return false; }
};

// this attachment should provide material data buffer for rendering
class MaterialAttachment : public gl::MaterialAttachment {
public:
	virtual ~MaterialAttachment();

	virtual bool init(StringView, const gl::BufferInfo &, Vector<Rc<gl::Material>> && = Vector<Rc<gl::Material>>());

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using gl::MaterialAttachment::init;
};

class MaterialAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~MaterialAttachmentHandle();

	virtual bool init(const Rc<Attachment> &, const FrameQueue &) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) override;

	const MaterialAttachment *getMaterialAttachment() const;

	const Rc<gl::MaterialSet> getSet() const;

protected:
	std::mutex _mutex;
	mutable Rc<gl::MaterialSet> _materials;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKATTACHMENT_H_ */
