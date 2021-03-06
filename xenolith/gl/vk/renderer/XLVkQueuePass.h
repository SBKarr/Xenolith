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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKQUEUEPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKQUEUEPASS_H_

#include "XLVkSync.h"
#include "XLVkObject.h"
#include "XLRenderQueueQueue.h"

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

class QueuePass : public renderqueue::Pass {
public:
	virtual ~QueuePass();

	virtual bool init(StringView, PassType, RenderOrdering, size_t subpassCount = 1);
	virtual void invalidate();

	QueueOperations getQueueOps() const { return _queueOps; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	QueueOperations _queueOps = QueueOperations::Graphics;
};

class QueuePassHandle : public renderqueue::PassHandle {
public:
	virtual ~QueuePassHandle();
	virtual void invalidate();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;
	virtual void finalize(FrameQueue &, bool) override;

	virtual QueueOperations getQueueOps() const;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &);
	virtual bool doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited);

	struct MaterialBuffers {
		Rc<DeviceBuffer> stagingBuffer;
		Rc<Buffer> targetBuffer;
		std::unordered_map<gl::MaterialId, uint32_t> ordering;
	};

	virtual MaterialBuffers updateMaterials(FrameHandle &iframe, const Rc<gl::MaterialSet> &data,
			const Vector<Rc<gl::Material>> &materials, SpanView<gl::MaterialId> dynamicMaterials, SpanView<gl::MaterialId> materialsToRemove);

	Function<void(bool)> _onPrepared;
	bool _valid = true;
	bool _commandsReady = false;
	bool _descriptorsReady = false;

	Device *_device = nullptr;
	Loop *_loop = nullptr;
	Rc<Fence> _fence;
	Rc<CommandPool> _pool;
	Rc<DeviceQueue> _queue;
	Vector<VkCommandBuffer> _buffers;
	Rc<FrameSync> _sync;
};

class VertexPass : public QueuePass {
public:
	virtual ~VertexPass();

	virtual bool init(StringView, RenderOrdering, size_t subpassCount = 1);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const VertexBufferAttachment *getVertexes() const { return _vertexes; }

protected:
	virtual void prepare(gl::Device &) override;

	const VertexBufferAttachment *_vertexes = nullptr;
};

class VertexPassHandle : public QueuePassHandle {
public:
	virtual ~VertexPassHandle();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(FrameHandle &) override;
	virtual bool doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited) override;

	VertexBufferAttachmentHandle *_mainBuffer;
};


}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKQUEUEPASS_H_ */
