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

#include "XLVkDeviceQueue.h"
#include "XLVkDevice.h"
#include "XLVkSync.h"

namespace stappler::xenolith::vk {

DeviceQueue::~DeviceQueue() { }

bool DeviceQueue::init(Device &device, VkQueue queue, uint32_t index, QueueOperations ops) {
	_device = &device;
	_queue = queue;
	_index = index;
	_ops = ops;
	return true;
}

bool DeviceQueue::submit(const gl::FrameSync &sync, Fence &fence, SpanView<VkCommandBuffer> buffers) {
	Vector<VkSemaphore> waitSem;
	Vector<VkPipelineStageFlags> waitStages;
	Vector<VkSemaphore> signalSem;

	for (auto &it : sync.waitAttachments) {
		auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

		if (!it.semaphore->isWaited()) {
			waitSem.emplace_back(sem);
			waitStages.emplace_back(VkPipelineStageFlags(it.stages));
		}
	}

	for (auto &it : sync.signalAttachments) {
		auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

		signalSem.emplace_back(sem);
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = waitSem.size();
	submitInfo.pWaitSemaphores = waitSem.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.commandBufferCount = buffers.size();
	submitInfo.pCommandBuffers = buffers.data();
	submitInfo.signalSemaphoreCount = signalSem.size();
	submitInfo.pSignalSemaphores = signalSem.data();

	log::vtext("Vk-Queue-Submit", (uintptr_t)_queue);

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
	});

	if (result == VK_SUCCESS) {
		// mark semaphores

		fence.setArmed(*this);

		for (auto &it : sync.waitAttachments) {
			it.semaphore->setWaited(true);
		}

		for (auto &it : sync.signalAttachments) {
			it.semaphore->setSignaled(true);
		}

		return true;
	}
	return false;
}

bool DeviceQueue::submit(Fence &fence, SpanView<VkCommandBuffer> buffers) {
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = buffers.size();
	submitInfo.pCommandBuffers = buffers.data();
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
	});

	if (result == VK_SUCCESS) {
		fence.setArmed(*this);
		return true;
	}
	return false;
}

void DeviceQueue::waitIdle() {
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		table.vkQueueWaitIdle(_queue);
	});
}

uint32_t DeviceQueue::getActiveFencesCount() {
	return _nfences.load();
}

void DeviceQueue::retainFence(const Fence &fence) {
	++ _nfences;
}

void DeviceQueue::releaseFence(const Fence &fence) {
	-- _nfences;
}

CommandPool::~CommandPool() {
	if (_commandPool) {
		log::vtext("VK-Error", "CommandPool was not destroyed");
	}
}

bool CommandPool::init(Device &dev, uint32_t familyIdx, QueueOperations c, bool transient) {
	_familyIdx = familyIdx;
	_class = c;
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = familyIdx;
	poolInfo.flags = transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	return dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool) == VK_SUCCESS;
}

void CommandPool::invalidate(Device &dev) {
	if (_commandPool) {
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);
		_commandPool = VK_NULL_HANDLE;
	} else {
		log::vtext("VK-Error", "CommandPool is not defined");
	}
}

VkCommandBuffer CommandPool::allocBuffer(Device &dev, Level level) {
	if (_commandPool) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VkCommandBufferLevel(level);
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer ret;
		if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, &ret) == VK_SUCCESS) {
			return ret;
		}
	}
	return nullptr;
}

Vector<VkCommandBuffer> CommandPool::allocBuffers(Device &dev, uint32_t count, Level level) {
	Vector<VkCommandBuffer> vec;
	if (_commandPool) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VkCommandBufferLevel(level);
		allocInfo.commandBufferCount = count;

		vec.resize(count);
		if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, vec.data()) == VK_SUCCESS) {
			return vec;
		}
		vec.clear();
	}
	return vec;
}

void CommandPool::freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &vec) {
	if (_commandPool) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(vec.size()), vec.data());
	}
	vec.clear();
}

void CommandPool::reset(Device &dev, bool release) {
	if (_commandPool) {
		dev.getTable()->vkResetCommandPool(dev.getDevice(), _commandPool, release ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);
	}
}

}
