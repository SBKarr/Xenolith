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

class Swapchain : public Ref {
public:
	virtual ~Swapchain();

	virtual bool init(const View *, Function<SwapchainConfig(const SurfaceInfo &)> &&);

	const View *getView() const { return _view; }

	virtual bool recreateSwapchain(Device &, gl::SwapchanCreationMode);
	virtual void invalidate(Device &);

	// invalidate all frames in process before that
	virtual void incrementGeneration(AppEvent::Value);

	virtual bool isBestPresentMode() const { return true; }

	virtual bool isResetRequired();

protected:
	bool _valid = false;
	uint64_t _order = 0;
	uint64_t _gen = 0;
	SwapchainConfig _config;
	Function<SwapchainConfig(const SurfaceInfo &)> _configCallback;
	Device *_device = nullptr;
	const View *_view = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLSWAPCHAIN_H_ */
