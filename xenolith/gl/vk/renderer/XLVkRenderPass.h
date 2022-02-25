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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_

#include "XLVkSync.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

class Device;
class SwapchainSync;
class DeviceBuffer;
class Swapchain;
class CommandPool;
class DeviceQueue;
class SwapchainAttachmentHandle;
class Framebuffer;
class VertexBufferAttachment;
class VertexBufferAttachmentHandle;

class RenderPass : public gl::RenderPass {
public:
	virtual ~RenderPass();

	virtual bool init(StringView, gl::RenderPassType, gl::RenderOrdering, size_t subpassCount = 1);
	virtual void invalidate();

	QueueOperations getQueueOps() const { return _queueOps; }

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(const gl::FrameQueue &) override;

protected:
	QueueOperations _queueOps = QueueOperations::Graphics;
};

class RenderPassHandle : public gl::RenderPassHandle {
public:
	virtual ~RenderPassHandle();
	virtual void invalidate();

	virtual bool prepare(gl::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(gl::FrameQueue &, Rc<gl::FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;
	virtual void finalize(gl::FrameQueue &, bool) override;

	virtual QueueOperations getQueueOps() const;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &);
	virtual bool doSubmit();

	struct MaterialBuffers {
		Rc<DeviceBuffer> stagingBuffer;
		Rc<Buffer> targetBuffer;
		std::unordered_map<gl::MaterialId, uint32_t> ordering;
	};

	virtual MaterialBuffers updateMaterials(gl::FrameHandle &iframe, const Rc<gl::MaterialSet> &data,
			const Vector<Rc<gl::Material>> &materials, SpanView<gl::MaterialId> dynamicMaterials, SpanView<gl::MaterialId> materialsToRemove);

	Function<void(bool)> _onPrepared;
	bool _valid = true;
	bool _commandsReady = false;
	bool _descriptorsReady = false;

	Device *_device = nullptr;
	Rc<Fence> _fence;
	Rc<CommandPool> _pool;
	Rc<DeviceQueue> _queue;
	Vector<VkCommandBuffer> _buffers;
	Rc<gl::FrameSync> _sync;
};

class VertexRenderPass : public RenderPass {
public:
	virtual ~VertexRenderPass();

	virtual bool init(StringView, gl::RenderOrdering, size_t subpassCount = 1);

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(const gl::FrameQueue &) override;

	const VertexBufferAttachment *getVertexes() const { return _vertexes; }

protected:
	virtual void prepare(gl::Device &) override;

	const VertexBufferAttachment *_vertexes = nullptr;
};

class VertexRenderPassHandle : public RenderPassHandle {
public:
	virtual ~VertexRenderPassHandle();

	virtual bool prepare(gl::FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &) override;
	virtual bool doSubmit() override;

	VertexBufferAttachmentHandle *_mainBuffer;
};


}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKRENDERPASS_H_ */
