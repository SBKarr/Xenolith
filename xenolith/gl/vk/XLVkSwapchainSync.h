/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_VK_XLVKSWAPCHAINSYNC_H_
#define XENOLITH_GL_VK_XLVKSWAPCHAINSYNC_H_

#include "XLVk.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::vk {

class Device;
class Swapchain;
class DeviceQueue;
class Fence;
class SwapchainHandle;
class Image;

class SwapchainSync : public gl::SwapchainImage {
public:
	virtual ~SwapchainSync();

	bool init(Device &dev, const Rc<SwapchainHandle> &, Mutex *mutex, uint64_t gen,
			Function<void(Rc<SwapchainSync> &&)> &&, Function<void(Rc<SwapchainSync> &&)> &&);
	void disable();
	void reset(Device &dev, const Rc<SwapchainHandle> &, uint64_t gen);
	void invalidate();

	virtual void cleanup() override;

	VkResult acquireImage(Device &, bool sync = false);

	VkResult submit(Device &dev, DeviceQueue &queue, gl::FrameSync &sync,
			Fence &fence, SpanView<VkCommandBuffer> buffers, gl::PipelineStage waitStage);
	VkResult present(Device &, DeviceQueue &);

	uint32_t getImageIndex() const { return _imageIndex; }
	void clearImageIndex();

	void setSubmitCompleted(bool);
	void setPresentationEnded(bool);

	Extent3 getImageExtent() const;

protected:
	VkResult doPresent(Device &, DeviceQueue &);

	void releaseForSwapchain();

	Mutex *_mutex = nullptr;
	Rc<SwapchainHandle> _handle;
	uint32_t _imageIndex = maxOf<uint32_t>();

	Function<void(Rc<SwapchainSync> &&)> _cleanup;
	Function<void(Rc<SwapchainSync> &&)> _present;

	bool _presented = false;
	bool _waitForSubmission = false;
	bool _waitForPresentation = false;
	VkResult _result = VK_SUCCESS;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAINSYNC_H_ */
