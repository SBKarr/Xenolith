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

#ifndef XENOLITH_GL_VK_XLVKVIEW_H_
#define XENOLITH_GL_VK_XLVKVIEW_H_

#include "XLVk.h"
#include "XLGlView.h"
#include "XLVkSwapchain.h"

namespace stappler::xenolith::vk {

class View : public gl::View {
public:
	virtual ~View();

	virtual bool init(Loop &, Device &, gl::ViewInfo &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;

	virtual void update() override;

	virtual void run() override;
	virtual void runWithQueue(const Rc<RenderQueue> &) override;

	virtual void onAdded(Device &);
	virtual void onRemoved();

	void deprecateSwapchain();

	virtual bool present(Rc<ImageStorage> &&) override;
	virtual bool presentImmediate(Rc<ImageStorage> &&) override;
	virtual void invalidateTarget(Rc<ImageStorage> &&) override;

	void captureImage(StringView, const Rc<gl::ImageObject> &image, AttachmentLayout l) const;

	void scheduleFence(Rc<Fence> &&);

	virtual uint64_t getUpdateInterval() const { return 0; }

protected:
	virtual bool pollInput();

	void invalidate();
	void scheduleSwapchainImage(uint64_t windowOffset);
	bool acquireScheduledImage(const Rc<SwapchainImage> &);

	bool recreateSwapchain(gl::PresentMode);
	bool createSwapchain(gl::SwapchainConfig &&cfg, gl::PresentMode presentMode);

	bool isImagePresentable(const gl::ImageObject &image, VkFilter &filter) const;

	void runWithSwapchainImage(Rc<ImageStorage> &&);
	void runScheduledPresent(Rc<SwapchainImage> &&);
	void presentWithQueue(DeviceQueue &, Rc<ImageStorage> &&);
	void invalidateSwapchainImage(Rc<ImageStorage> &&);

	void updateFrameInterval();

	void waitForFences(uint64_t min);

	bool _blockDeprecation = false;
	uint64_t _fenceOrder = 0;
	uint64_t _frameOrder = 0;
	Rc<Surface> _surface;
	Rc<Instance> _instance;
	Rc<Device> _device;
	Rc<SwapchainHandle> _swapchain;
	String _threadName;

	Rc<ImageStorage> _initImage;
	Set<Rc<Fence>> _fences;

	Vector<Rc<SwapchainImage>> _fenceImages;
	Vector<Rc<SwapchainImage>> _scheduledImages;
	Vector<Rc<SwapchainImage>> _scheduledPresent;
};

}

#endif /* XENOLITH_GL_VK_XLVKVIEW_H_ */
