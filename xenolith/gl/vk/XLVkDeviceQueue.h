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
#include "XLGlFrameHandle.h"

namespace stappler::xenolith::vk {

class Device;
class DeviceQueue;
class CommandPool;
class Semaphore;

struct DeviceQueueFamily {
	struct Waiter {
		Function<void(gl::Loop &, const Rc<DeviceQueue> &)> acquireForLoop;
		Function<void(gl::Loop &)> releaseForLoop;
		Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> acquireForFrame;
		Function<void(gl::FrameHandle &)> releaseForFrame;

		Rc<gl::FrameHandle> handle;
		Rc<gl::Loop> loop;
		Rc<Ref> ref;

		Waiter(Function<void(gl::FrameHandle &, const Rc<DeviceQueue> &)> &&a, Function<void(gl::FrameHandle &)> &&r,
				gl::FrameHandle *h, Rc<Ref> &&ref)
		: acquireForFrame(move(a)), releaseForFrame(move(r)), handle(h), ref(move(ref)) { }

		Waiter(Function<void(gl::Loop &, const Rc<DeviceQueue> &)> &&a, Function<void(gl::Loop &)> &&r,
				gl::Loop *h, Rc<Ref> &&ref)
		: acquireForLoop(move(a)), releaseForLoop(move(r)), loop(h), ref(move(ref)) { }
	};

	uint32_t index;
	uint32_t count;
	QueueOperations preferred = QueueOperations::None;
	QueueOperations ops = QueueOperations::None;
	VkExtent3D transferGranularity;
	Vector<VkQueue> queues;
	Vector<Rc<CommandPool>> pools;
	Vector<Waiter> waiters;
};

class DeviceQueue : public Ref {
public:
	virtual ~DeviceQueue();

	virtual bool init(Device &, VkQueue, uint32_t, QueueOperations);

	uint32_t getIndex() const { return _index; }
	VkQueue getQueue() const { return _queue; }
	QueueOperations getOps() const { return _ops; }

protected:
	Rc<Device> _device;
	uint32_t _index;
	QueueOperations _ops = QueueOperations::None;
	VkQueue _queue;
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
