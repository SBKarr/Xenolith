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

class SwapchainSync : public Ref {
public:
	virtual ~SwapchainSync();

	bool init(Device &dev, const Rc<SwapchainHandle> &, Mutex *mutex, uint64_t gen, Function<void(Rc<SwapchainSync> &&)> &&);
	void reset(Device &dev, const Rc<SwapchainHandle> &, uint64_t gen);
	void invalidate();

	VkResult acquireImage(Device &, bool sync = false);
	VkResult present(Device &, DeviceQueue &);

	VkResult submitWithPresent(Device &, DeviceQueue &, gl::FrameSync &,
			Fence &, SpanView<VkCommandBuffer>, gl::PipelineStage waitStage);

	uint32_t getImageIndex() const { return _imageIndex; }
	void clearImageIndex();

	Rc<Image> getImage() const;

	void setSubmitCompleted(bool);
	void setPresentationEnded(bool);

protected:
	VkResult doPresent(Device &, DeviceQueue &);

	void releaseForSwapchain();

	Mutex *_mutex;
	Rc<SwapchainHandle> _handle;
	uint32_t _imageIndex = maxOf<uint32_t>();
	uint64_t _gen = 0;
	Rc<Semaphore> _imageReady;
	Rc<Semaphore> _renderFinished;

	Function<void(Rc<SwapchainSync> &&)> _cleanup;

	bool _waitForSubmission = false;
	bool _waitForPresentation = false;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAINSYNC_H_ */
