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


#ifndef XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_
#define XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_

#include "XLVkInstance.h"
#include "XLGlDevice.h"
#include "XLGlLoop.h"
#include "XLRenderQueueFrameHandle.h"

namespace stappler::xenolith::vk {

class Device;
class DeviceQueue;
class CommandPool;
class Semaphore;
class Fence;
class Loop;

struct DeviceQueueFamily {
	using FrameHandle = renderqueue::FrameHandle;

	struct Waiter {
		Function<void(Loop &, const Rc<DeviceQueue> &)> acquireForLoop;
		Function<void(Loop &)> releaseForLoop;
		Function<void(FrameHandle &, const Rc<DeviceQueue> &)> acquireForFrame;
		Function<void(FrameHandle &)> releaseForFrame;

		Rc<FrameHandle> handle;
		Rc<Loop> loop;
		Rc<Ref> ref;

		Waiter(Function<void(FrameHandle &, const Rc<DeviceQueue> &)> &&a, Function<void(FrameHandle &)> &&r,
				FrameHandle *h, Rc<Ref> &&ref)
		: acquireForFrame(move(a)), releaseForFrame(move(r)), handle(h), ref(move(ref)) { }

		Waiter(Function<void(Loop &, const Rc<DeviceQueue> &)> &&a, Function<void(Loop &)> &&r,
				Loop *h, Rc<Ref> &&ref)
		: acquireForLoop(move(a)), releaseForLoop(move(r)), loop(h), ref(move(ref)) { }
	};

	uint32_t index;
	uint32_t count;
	QueueOperations preferred = QueueOperations::None;
	QueueOperations ops = QueueOperations::None;
	VkExtent3D transferGranularity;
	Vector<Rc<DeviceQueue>> queues;
	Vector<Rc<CommandPool>> pools;
	Vector<Waiter> waiters;
};

class DeviceQueue final : public Ref {
public:
	using FrameSync = renderqueue::FrameSync;
	using FrameHandle = renderqueue::FrameHandle;

	virtual ~DeviceQueue();

	virtual bool init(Device &, VkQueue, uint32_t, QueueOperations);

	bool submit(const FrameSync &, Fence &, SpanView<VkCommandBuffer>);
	bool submit(Fence &, SpanView<VkCommandBuffer>);

	void waitIdle();

	uint32_t getActiveFencesCount();
	void retainFence(const Fence &);
	void releaseFence(const Fence &);

	uint32_t getIndex() const { return _index; }
	uint64_t getFrameIndex() const { return _frameIdx; }
	VkQueue getQueue() const { return _queue; }
	QueueOperations getOps() const { return _ops; }
	VkResult getResult() const { return _result; }

	void setOwner(FrameHandle &);
	void reset();

protected:
	Device *_device = nullptr;
	uint32_t _index = 0;
	uint64_t _frameIdx = 0;
	QueueOperations _ops = QueueOperations::None;
	VkQueue _queue;
	std::atomic<uint32_t> _nfences;
	VkResult _result = VK_ERROR_UNKNOWN;
};

enum class BufferLevel {
	Primary = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	Secondary = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
};

class CommandPool : public Ref {
public:
	using Level = BufferLevel;

	virtual ~CommandPool();

	bool init(Device &dev, uint32_t familyIdx, QueueOperations c = QueueOperations::Graphics, bool transient = true);
	void invalidate(Device &dev);

	QueueOperations getClass() const { return _class; }
	uint32_t getFamilyIdx() const { return _familyIdx; }
	VkCommandPool getCommandPool() const { return _commandPool; }

	VkCommandBuffer allocBuffer(Device &dev, Level = Level::Primary);
	Vector<VkCommandBuffer> allocBuffers(Device &dev, uint32_t, Level = Level::Secondary);
	void freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &);
	void reset(Device &dev, bool release = false);

protected:
	uint32_t _familyIdx = 0;
	uint32_t _currentComplexity = 0;
	uint32_t _bestComplexity = 0;
	QueueOperations _class = QueueOperations::Graphics;
	VkCommandPool _commandPool = VK_NULL_HANDLE;
};

}

#endif /* XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_ */
