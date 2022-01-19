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

#include "XLVkFrame.h"
#include "XLVkDevice.h"
#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

bool FrameHandle::init(gl::Loop &loop, gl::Swapchain &swapchain, gl::RenderQueue &queue, uint32_t gen, bool readyForSubmit) {
	if (!gl::FrameHandle::init(loop, swapchain, queue, gen, readyForSubmit)) {
		return false;
	}

	_memPool = Rc<DeviceMemoryPool>::create(((Device *)_device)->getAllocator(), true);
	return true;
}

bool FrameHandle::init(gl::Loop &loop, gl::RenderQueue &queue, uint32_t gen) {
	if (!gl::FrameHandle::init(loop, queue, gen)) {
		return false;
	}

	_memPool = Rc<DeviceMemoryPool>::create(((Device *)_device)->getAllocator(), true);
	return true;
}

Rc<SwapchainSync> FrameHandle::acquireSwapchainSync() {
	if (!_swapchainSync) {
		_swapchainSync = ((Swapchain *)_swapchain)->acquireSwapchainSync(*((Device *)_device), _order);
	}
	return _swapchainSync;
}

void FrameHandle::invalidateSwapchain() {
	if (_swapchainSync) {
		_swapchainSync->lock();
		_swapchainSync->setSwapchainValid(false);
		_swapchainSync->unlock();
	}
}

}
