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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKRENDERQUEUEATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKRENDERQUEUEATTACHMENT_H_

#include "XLVkRenderPass.h"
#include "XLVkDeviceQueue.h"
#include "XLVkTransferAttachment.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith::vk {

class RenderQueueAttachment;

class RenderQueueCompiler : public gl::RenderQueue {
public:
	virtual ~RenderQueueCompiler();

	bool init(Device &);

	RenderQueueAttachment *getAttachment() const { return _attachment; }

protected:
	RenderQueueAttachment *_attachment;
};

struct RenderQueueInput : public gl::AttachmentInputData {
	Rc<gl::RenderQueue> queue;
};

class RenderQueueAttachment : public gl::GenericAttachment {
public:
	virtual ~RenderQueueAttachment();

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &) override;
};

class RenderQueueAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~RenderQueueAttachmentHandle();

	virtual bool setup(gl::FrameHandle &handle) override;
	virtual bool submitInput(gl::FrameHandle &, Rc<gl::AttachmentInputData> &&) override;

	const Rc<gl::RenderQueue> &getRenderQueue() const { return _input->queue; }
	const Rc<TransferResource> &getTransferResource() const { return _resource; }

protected:
	void runShaders(gl::FrameHandle &frame);
	void runPipelines(gl::FrameHandle &frame);
	void fail();
	void complete();

	Device *_device = nullptr;
	std::atomic<size_t> _programsInQueue = 0;
	std::atomic<size_t> _pipelinesInQueue = 0;
	Rc<TransferResource> _resource;
	Rc<RenderQueueInput> _input;
};

class RenderQueueRenderPass : public RenderPass {
public:
	virtual ~RenderQueueRenderPass();

	virtual bool init(StringView);

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &) override;

	const RenderQueueAttachment *getAttachment() const {
		return _attachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	RenderQueueAttachment *_attachment = nullptr;
};

class RenderQueueRenderPassHandle : public RenderPassHandle {
public:
	virtual ~RenderQueueRenderPassHandle();

	virtual bool prepare(gl::FrameHandle &) override;
	virtual void submit(gl::FrameHandle &, Function<void(const Rc<gl::RenderPass> &)> &&) override;

protected:
	virtual void addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) override;

	Rc<TransferResource> _resource;
	Rc<gl::RenderQueue> _queue;
	RenderQueueAttachmentHandle *_attachment;

	//Rc<CommandPool> _resourcePool;
	//Rc<CommandPool> _materialPool;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKRENDERQUEUEATTACHMENT_H_ */
