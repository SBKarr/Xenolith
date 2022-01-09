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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKRENDERFONTATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKRENDERFONTATTACHMENT_H_

#include "XLVkRenderPass.h"

namespace stappler::xenolith::vk {

class RenderFontAttachment : public gl::GenericAttachment {
public:
	virtual ~RenderFontAttachment();

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &) override;
};

class RenderFontAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~RenderFontAttachmentHandle();

	virtual bool setup(gl::FrameHandle &handle) override;
	virtual bool submitInput(gl::FrameHandle &, Rc<gl::AttachmentInputData> &&) override;

protected:
	void writeBufferData(gl::RenderFontInput::FontRequest &, VkBufferImageCopy *target);
	void addBufferData(Vector<VkBufferImageCopy> &&);
	void writeAtlasData(gl::FrameHandle &);

	uint32_t nextBufferOffset(size_t blockSize);

	Rc<gl::RenderFontInput> _input;
	gl::ImageData _image;
	uint32_t _counter = 0;
	VkDeviceSize _bufferSize = 0;
	VkDeviceSize _optimalRowAlignment = 1;
	VkDeviceSize _optimalTextureAlignment = 1;
	std::atomic<size_t> _bufferOffset = 0;
	Rc<DeviceBuffer> _frontBuffer;
	Vector<VkBufferImageCopy> _bufferData;
	Mutex _mutex;
};

class RenderFontRenderPass : public RenderPass {
public:
	virtual ~RenderFontRenderPass();

	virtual bool init(StringView);

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &) override;

	const RenderFontAttachment *getRenderFontAttachment() const {
		return _fontAttachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	const RenderFontAttachment *_fontAttachment;
};

class RenderFontRenderPassHandle : public RenderPassHandle {
public:
	virtual ~RenderFontRenderPassHandle();

	virtual bool init(gl::RenderPass &, gl::RenderPassData *, const gl::FrameHandle &) override;

	virtual QueueOperations getQueueOps() const override;

	virtual bool prepare(gl::FrameHandle &) override;

protected:
	virtual void addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) override;
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &, uint32_t index) override;

	RenderFontAttachmentHandle *_fontAttachment;
	QueueOperations _queueOps = QueueOperations::None;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKRENDERFONTATTACHMENT_H_ */
