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

#ifndef XENOLITH_GL_VK_XLVKSWAPCHAIN_H_
#define XENOLITH_GL_VK_XLVKSWAPCHAIN_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"
#include "XLRenderQueueImageStorage.h"

namespace stappler::xenolith::vk {

class SwapchainImage;

class Surface : public Ref {
public:
	virtual ~Surface();

	bool init(Instance *instance, VkSurfaceKHR surface, Ref *);

	VkSurfaceKHR getSurface() const { return _surface; }

protected:
	Rc<Ref> _window;
	Rc<Instance> _instance;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
};

class SwapchainHandle : public gl::Object {
public:
	using ImageStorage = renderqueue::ImageStorage;

	struct SwapchainImageData {
		Rc<Image> image;
		Map<gl::ImageViewInfo, Rc<ImageView>> views;
	};

	virtual ~SwapchainHandle();

	bool init(Device &dev, const gl::SwapchainConfig &, gl::ImageInfo &&, gl::PresentMode,
			Surface *, uint32_t families[2], SwapchainHandle * = nullptr);

	gl::PresentMode getPresentMode() const { return _presentMode; }
	gl::PresentMode getRebuildMode() const { return _rebuildMode; }
	const gl::ImageInfo &getImageInfo() const { return _imageInfo; }
	const gl::SwapchainConfig &getConfig() const { return _config; }
	VkSwapchainKHR getSwapchain() const { return _swapchain; }
	uint32_t getAcquiredImagesCount() const { return _acquiredImages; }
	const Vector<SwapchainImageData> &getImages() const { return _images; }

	bool isDeprecated();
	bool isOptimal() const;

	// returns true if it was first deprecation
	bool deprecate();

	Rc<ImageStorage> acquire(bool lockfree, const Rc<Fence> &fence);
	bool acquire(const Rc<SwapchainImage> &, const Rc<Fence> &fence, bool immediate);

	VkResult present(DeviceQueue &queue, const Rc<ImageStorage> &);
	void invalidateImage(const Rc<ImageStorage> &);

	Rc<Semaphore> acquireSemaphore();
	void releaseSemaphore(Rc<Semaphore> &&);

	Rc<gl::ImageView> makeView(const Rc<gl::ImageObject> &, const gl::ImageViewInfo &);

protected:
	using gl::Object::init;

	gl::ImageViewInfo getSwapchainImageViewInfo(const gl::ImageInfo &image) const;

	Device *_device = nullptr;
	bool _deprecated = false;
	gl::PresentMode _presentMode = gl::PresentMode::Unsupported;
	gl::ImageInfo _imageInfo;
	gl::SwapchainConfig _config;
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
	Vector<SwapchainImageData> _images;
	uint32_t _acquiredImages = 0;
	uint64_t _presentedFrames = 0;
	uint64_t _presentTime = 0;
	gl::PresentMode _rebuildMode = gl::PresentMode::Unsupported;

	Mutex _resourceMutex;
	Vector<Rc<Semaphore>> _semaphores;
	Rc<Semaphore> _presentSemaphore;
	Rc<Surface> _surface;
};

class SwapchainImage : public renderqueue::ImageStorage {
public:
	enum class State {
		Initial,
		Submitted,
		Presented,
	};

	virtual ~SwapchainImage();

	virtual bool init(Rc<SwapchainHandle> &&, uint64_t frameOrder, uint64_t presentWindow);
	virtual bool init(Rc<SwapchainHandle> &&, const SwapchainHandle::SwapchainImageData &, const Rc<Semaphore> &);

	virtual void cleanup() override;
	virtual void rearmSemaphores(gl::Loop &) override;
	virtual void releaseSemaphore(gl::Semaphore *) override;

	virtual bool isSemaphorePersistent() const override { return false; }

	virtual gl::ImageInfoData getInfo() const override;

	virtual Rc<gl::ImageView> makeView(const gl::ImageViewInfo &) override;

	void setImage(Rc<SwapchainHandle> &&, const SwapchainHandle::SwapchainImageData &, const Rc<Semaphore> &);

	uint64_t getOrder() const { return _order; }
	uint64_t getPresentWindow() const { return _presentWindow; }

	void setPresented();
	bool isPresented() const { return _state == State::Presented; }

	const Rc<SwapchainHandle> &getSwapchain() const { return _swapchain; }

	void invalidateImage();
	void invalidateSwapchain();

protected:
	using renderqueue::ImageStorage::init;

	uint64_t _order = 0;
	uint64_t _presentWindow = 0;
	State _state = State::Initial;
	Rc<SwapchainHandle> _swapchain;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAIN_H_ */
