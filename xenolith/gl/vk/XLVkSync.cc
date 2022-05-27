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
		return gl::Semaphore::init(dev, Semaphore_destroy, gl::ObjectType::Semaphore, _sem);
	}

	return false;
}

Fence::~Fence() { }

bool Fence::init(Device &dev) {
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = 0;

	_state = Disabled;

	if (dev.getTable()->vkCreateFence(dev.getDevice(), &fenceInfo, nullptr, &_fence) == VK_SUCCESS) {
		return gl::Object::init(dev, Fence_destroy, gl::ObjectType::Fence, _fence);
	}
	return false;
}

void Fence::setArmed(DeviceQueue &q) {
	std::unique_lock<Mutex> lock(_mutex);
	_state = Armed;
	_queue = &q;
	_queue->retainFence(*this);
	_armedTime = platform::device::_clock(platform::device::ClockType::Monotonic);
}

void Fence::setTag(StringView tag) {
	_tag = tag;
}

void Fence::addRelease(Function<void(bool)> &&cb, Ref *ref, StringView tag) {
	std::unique_lock<Mutex> lock(_mutex);
	_release.emplace_back(ReleaseHandle({move(cb), ref, tag}));
}

bool Fence::check(gl::Loop &loop, bool lockfree) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		if (_state == Delayed) {
			_state = Signaled;
			doRelease(true);
		}
		return true;
	}

	auto dev = ((Device *)_device);
	enum VkResult status;

	dev->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		if (lockfree) {
			status = table.vkGetFenceStatus(device, _fence);
		} else {
			status = table.vkWaitForFences(device, 1, &_fence, VK_TRUE, UINT64_MAX);
		}
	});

	switch (status) {
	case VK_SUCCESS:
		_state = Signaled;
		// log::vtext("vk::Fence", "[", loop.getClock(), "] ",  _tag, ": signaled: ", platform::device::_clock(platform::device::ClockType::Monotonic) - _armedTime);
		doRelease(true);
		return true;
		break;
	case VK_TIMEOUT:
	case VK_NOT_READY:
		_state = Armed;
		return false;
	default:
		break;
	}
	return false;
}

bool Fence::checkDelayed(bool lockfree) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_state != Armed) {
		return true;
	}

	auto dev = ((Device *)_device);
	enum VkResult status;

	if (lockfree) {
		status = dev->getTable()->vkGetFenceStatus(dev->getDevice(), _fence);
	} else {
		status = dev->getTable()->vkWaitForFences(dev->getDevice(), 1, &_fence, VK_TRUE, UINT64_MAX);
	}

	switch (status) {
	case VK_SUCCESS:
		_state = Delayed;
		return true;
		break;
	case VK_TIMEOUT:
	case VK_NOT_READY:
		_state = Armed;
		return false;
	default:
		break;
	}
	return false;
}

void Fence::reset(gl::Loop &loop, Function<void(Rc<Fence> &&)> &&cb) {
	std::unique_lock<Mutex> lock(_mutex);

	_state = Disabled;

	doRelease(false);

	auto refId = retain();
	loop.getQueue()->perform(Rc<Task>::create([this] (const Task &) {
		auto dev = ((Device *)_device);
		dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
		return true;
	}, [this, cb = move(cb), refId] (const Task &, bool success) {
		if (success) {
			cb(this);
		}
		release(refId);
	}));
}

void Fence::resetUnsafe() {
	std::unique_lock<Mutex> lock(_mutex);

	_state = Disabled;

	doRelease(false);

	auto dev = ((Device *)_device);
	dev->getTable()->vkResetFences(dev->getDevice(), 1, &_fence);
}

void Fence::doRelease(bool success) {
	if (_queue) {
		_queue->releaseFence(*this);
		_queue = nullptr;
	}
	if (!_release.empty()) {
		XL_PROFILE_BEGIN(total, "vk::Fence::reset", "total", 250);
		for (auto &it : _release) {
			if (it.callback) {
				XL_PROFILE_BEGIN(fence, "vk::Fence::reset", it.tag, 250);
				it.callback(success);
				XL_PROFILE_END(fence);
			}
		}
		XL_PROFILE_END(total);
		_release.clear();
	}

	_tag = StringView();
}


}
