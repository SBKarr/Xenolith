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

#ifndef XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_
#define XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_

#include "XLGl.h"
#include "XLPlatform.h"

namespace stappler::xenolith::gl {

class FrameHandle;
class FrameCacheStorage;
struct FrameQueueAttachmentData;

struct SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = platform::graphic::getCommonFormat();
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;

	String description() const;
};

struct SurfaceInfo {
	uint32_t minImageCount;
	uint32_t maxImageCount;
	Extent2 currentExtent;
	Extent2 minImageExtent;
	Extent2 maxImageExtent;
	uint32_t maxImageArrayLayers;
	CompositeAlphaFlags supportedCompositeAlpha;
	SurfaceTransformFlags supportedTransforms;
	SurfaceTransformFlags currentTransform;
	ImageUsage supportedUsageFlags;
	Vector<Pair<ImageFormat, ColorSpace>> formats;
	Vector<PresentMode> presentModes;

	bool isSupported(const SwapchainConfig &) const;

	String description() const;
};

class SwapchainImage : public Ref {
public:
	virtual ~SwapchainImage();

	virtual bool init(Device &dev);

	Rc<gl::ImageAttachmentObject> getImage() const { return _image; }

protected:
	virtual void setImage(const Rc<gl::ImageAttachmentObject> &);

	uint64_t _gen = 0;
	Rc<Semaphore> _imageReady;
	Rc<Semaphore> _renderFinished;

	gl::ImageAttachmentObject *_image = nullptr;
};

class Swapchain : public Ref {
public:
	struct PresentTask : public Ref {
		Rc<FrameCacheStorage> cache;
		const gl::ImageAttachment *attachment;
		Rc<gl::ImageAttachmentObject> object;
		uint64_t order = maxOf<uint64_t>();

		PresentTask(const Rc<FrameCacheStorage> &c, const gl::ImageAttachment *a, const Rc<gl::ImageAttachmentObject> &obj)
		: cache(c), attachment(a), object(obj) { }

		virtual ~PresentTask();
	};

	virtual ~Swapchain();

	virtual bool init(View *);

	const View *getView() const { return _view; }

	uint64_t getGen() const { return _gen; }

	virtual bool recreateSwapchain(Device &, gl::SwapchanCreationMode);
	virtual void invalidate(Device &);

	// true - if presentation request accepted, false otherwise,
	// frame should not mark image as detached if false is returned
	virtual bool present(gl::Loop &, const Rc<PresentTask> &);

	// invalidate all frames in process before that
	virtual void deprecate();

	virtual bool isBestPresentMode() const { return true; }

	virtual bool isResetRequired();

	gl::ImageInfo getSwapchainImageInfo() const;
	gl::ImageInfo getSwapchainImageInfo(const gl::SwapchainConfig &cfg) const;
	gl::ImageViewInfo getSwapchainImageViewInfo(const gl::ImageInfo &image) const;

	virtual Rc<gl::ImageAttachmentObject> acquireImage(const Loop &loop, const ImageAttachment *a, Extent3 e);

protected:
	uint64_t _order = 0;
	uint64_t _gen = 0;
	SwapchainConfig _config;
	Device *_device = nullptr;
	View *_view = nullptr;

	mutable Mutex _swapchainMutex;
	Mutex _presentCurrentMutex;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_ */
