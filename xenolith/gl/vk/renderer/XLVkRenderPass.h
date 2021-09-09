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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_

#include "XLVkSync.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

class Device;

class RenderPass : public gl::RenderPass {
public:
	virtual ~RenderPass();
	virtual void invalidate();

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &);

protected:
};

class RenderPassHandle : public gl::RenderPassHandle {
public:
	struct Sync {
		Vector<Rc<gl::AttachmentHandle>> waitAttachment;
		Vector<VkSemaphore> waitSem;
		Vector<VkPipelineStageFlags> waitStages;
		Vector<VkSemaphore> signalSem;
		Vector<Rc<gl::AttachmentHandle>> signalAttachment;
		Vector<Rc<SwapchainSync>> swapchainSync;
	};

	virtual ~RenderPassHandle();
	virtual void invalidate();

	// if submit is true - do run + submit in one call
	virtual bool prepare(gl::FrameHandle &) override;
	virtual void submit(gl::FrameHandle &, Function<void(const Rc<gl::RenderPass> &)> &&) override;

protected:
	virtual bool doPrepareDescriptors(gl::FrameHandle &, uint32_t index);
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &, uint32_t index);

	virtual bool present(gl::FrameHandle &);

	virtual Sync makeSyncInfo();

	bool _commandsReady = false;
	bool _descriptorsReady = false;

	Device *_device = nullptr;
	Rc<Fence> _fence;
	Rc<CommandPool> _pool;
	Rc<DeviceQueue> _queue;
	Vector<VkCommandBuffer> _buffers;

	Rc<SwapchainAttachmentHandle> _presentAttachment;
	Sync _sync;
};

class VertexRenderPass : public RenderPass {
public:
	virtual ~VertexRenderPass();

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &);
};

class VertexBufferAttachmentHandle;

class VertexRenderPassHandle : public RenderPassHandle {
public:
	virtual ~VertexRenderPassHandle();

protected:
	virtual void addRequiredAttachment(const gl::Attachment *, const Rc<gl::AttachmentHandle> &);
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &, uint32_t index) override;

	VertexBufferAttachmentHandle *_mainBuffer;
};


}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_ */
