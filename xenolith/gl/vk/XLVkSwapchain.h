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

#include "XLVk.h"
#include "XLGlSwapchain.h"
#include "XLVkImageAttachment.h"

namespace stappler::xenolith::vk {

class Device;
class ImageView;
class Framebuffer;
class Fence;
class RenderPassImpl;
class FrameHandle;

class SwapchainSync : public Ref {
public:
	virtual ~SwapchainSync();

	bool init(Device &dev, uint32_t idx, uint64_t gen);
	void reset(uint64_t gen);
	void invalidate();

	void lock();
	void unlock();

	VkResult acquireImage(Device &, Swapchain &);

	void setSwapchainValid(bool value) { _swapchainValid = value; }
	bool isSwapchainValid() const { return _swapchainValid; }

	uint32_t getFrameIndex() const { return _frameIndex; }
	uint32_t getImageIndex() const { return _imageIndex; }
	void clearImageIndex();

	const Rc<Semaphore> &getImageReady() const { return _imageReady; }
	const Rc<Semaphore> &getRenderFinished() const { return _renderFinished; }

protected:
	Mutex _mutex;
	bool _swapchainValid = true;
	uint32_t _frameIndex = 0;
	uint32_t _imageIndex = maxOf<uint32_t>();
	uint64_t _gen = 0;
	Rc<Semaphore> _imageReady;
	Rc<Semaphore> _renderFinished;
};

class Swapchain : public gl::Swapchain {
public:
	static constexpr uint32_t FramesInFlight = 2;

	virtual ~Swapchain();

	bool init(const gl::View *, Device &, Function<gl::SwapchainConfig(const gl::SurfaceInfo &)> &&, VkSurfaceKHR);

	virtual bool recreateSwapchain(gl::Device &, gl::SwapchanCreationMode) override;
	virtual void invalidate(gl::Device &) override;

	bool createSwapchain(Device &, gl::SwapchainConfig &&, gl::PresentMode);
	void cleanupSwapchain(Device &);

	gl::PresentMode getPresentMode() const { return _presentMode; }
	VkSurfaceKHR getSurface() const { return _surface; }
	VkSwapchainKHR getSwapchain() const { return _swapchain; }
	gl::ImageInfo getSwapchainImageInfo(const gl::SwapchainConfig &cfg) const;
	gl::ImageViewInfo getSwapchainImageViewInfo(const gl::ImageInfo &image) const;

	virtual bool isBestPresentMode() const override;

	Rc<SwapchainSync> acquireSwapchainSync(Device &, uint64_t);
	void releaseSwapchainSync(Rc<SwapchainSync> &&);

	bool present(gl::Loop &loop, Rc<Image> &&, VkImageLayout l);

	Rc<Image> getImage(uint32_t) const;

protected:
	gl::PresentMode _presentMode = gl::PresentMode::Fifo;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
	VkSwapchainKHR _oldSwapchain = VK_NULL_HANDLE;

	Vector<Vector<Rc<SwapchainSync>>> _sems;
	Vector<Rc<Image>> _images;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAIN_H_ */
