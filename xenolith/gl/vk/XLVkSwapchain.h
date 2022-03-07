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

#ifndef XENOLITH_GL_VK_XLVKSWAPCHAIN_H_
#define XENOLITH_GL_VK_XLVKSWAPCHAIN_H_

#include "XLGlSwapchain.h"
#include "XLVkSwapchainSync.h"
#include "XLVkAttachment.h"

namespace stappler::xenolith::vk {

class Device;
class ImageView;
class Framebuffer;
class Fence;
class RenderPassImpl;
class FrameHandle;

class Swapchain : public gl::Swapchain {
public:
	static constexpr uint32_t FramesInFlight = 2;

	virtual ~Swapchain();

	bool init(gl::View *, Device &, VkSurfaceKHR);

	virtual bool recreateSwapchain(gl::Device &, gl::SwapchanCreationMode) override;
	virtual void invalidate(gl::Device &) override;

	virtual bool present(gl::Loop &, const Rc<PresentTask> &) override;

	virtual void deprecate() override;

	bool createSwapchain(Device &, gl::SwapchainConfig &&, gl::PresentMode);

	VkSurfaceKHR getSurface() const { return _surface; }
	Rc<SwapchainHandle> getSwapchain() const { return _swapchain; }

	Rc<SwapchainSync> acquireSwapchainSync(Device &, bool lock = true);
	void releaseSync(Rc<SwapchainSync> &&);
	void presentSync(Rc<SwapchainSync> &&);

	virtual Rc<gl::ImageAttachmentObject> acquireImage(const gl::Loop &loop, const gl::ImageAttachment *a, Extent3 e) override;

protected:
	void onPresentComplete(gl::Loop &, VkResult res, const Rc<PresentTask> &);

	bool isImagePresentable(const gl::ImageObject &image, VkFilter &filter) const;

	bool presentImmediate(const Rc<PresentTask> &);

	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	Rc<SwapchainHandle> _swapchain;

	Vector<Rc<SwapchainSync>> _syncs;
	Rc<SwapchainSync> _presentedSync;

	bool _presentScheduled = false;
	Rc<PresentTask> _presentPending;
	Rc<PresentTask> _presentCurrent;
	Rc<DeviceQueue> _presentQueue;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAIN_H_ */
