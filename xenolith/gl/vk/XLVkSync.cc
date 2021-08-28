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

#include "XLVkSync.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

Semaphore::~Semaphore() { }

static void Semaphore_destroy(gl::Device *dev, gl::ObjectType, void *ptr) {
	auto d = ((Device *)dev);
	d->getTable()->vkDestroySemaphore(d->getDevice(), (VkSemaphore)ptr, nullptr);
}

static void Fence_destroy(gl::Device *dev, gl::ObjectType, void *ptr) {
	auto d = ((Device *)dev);
	d->getTable()->vkDestroyFence(d->getDevice(), (VkFence)ptr, nullptr);
}

bool Semaphore::init(Device &dev) {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = 0;

	if (dev.getTable()->vkCreateSemaphore(dev.getDevice(), &semaphoreInfo, nullptr, &_sem) == VK_SUCCESS) {
		return gl::Object::init(dev, Semaphore_destroy, gl::ObjectType::Semaphore, _sem);
	}

	return false;
}

void Semaphore::setSignaled(bool signaled) {
	_signaled = signaled;
}

VkSemaphore Semaphore::getUnsignalled() {
	if (_signaled) {
		reset();
	}
	return _sem;
}

void Semaphore::reset() {
	if (_signaled) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreInfo.pNext = nullptr;
		semaphoreInfo.flags = 0;

		auto dev = ((Device *)_device);
		dev->getTable()->vkDestroySemaphore(dev->getDevice(), (VkSemaphore)_ptr, nullptr);
		dev->getTable()->vkCreateSemaphore(dev->getDevice(), &semaphoreInfo, nullptr, &_sem);
		_ptr = _sem;
		_signaled = false;
	}
}

Fence::~Fence() { }

bool Fence::init(Device &dev) {
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = 0;

	if (dev.getTable()->vkCreateFence(dev.getDevice(), &fenceInfo, nullptr, &_fence) == VK_SUCCESS) {
		return gl::Object::init(dev, Fence_destroy, gl::ObjectType::Fence, _fence);
	}
	return false;
}

void Fence::addRelease(Function<void()> &&cb, Ref *ref) {
	_release.emplace_back(ReleaseHandle({move(cb), ref}));
}

bool Fence::check(bool lockfree) {
	auto dev = ((Device *)_device);
	enum VkResult status;
	if (lockfree) {
		status = dev->getTable()->vkGetFenceStatus(dev->getDevice(), _fence);
	} else {
		status = dev->getTable()->vkWaitForFences(dev->getDevice(), 1, &_fence, VK_TRUE, UINT64_MAX);
	}
	switch (status) {
	case VK_SUCCESS:
		_signaled = true;
		for (auto &it : _release) {
			if (it.callback) {
				it.callback();
			}
		}
		_release.clear();
		return true;
		break;
	case VK_TIMEOUT:
	case VK_NOT_READY:
		_signaled = false;
		return false;
	default:
		break;
	}
	return false;
}

void Fence::reset() {
	if (!_release.empty()) {
		for (auto &it : _release) {
			it.callback();
		}
		_release.clear();
	}

	auto dev = ((Device *)_device);
	dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
	_signaled = false;
}


}
