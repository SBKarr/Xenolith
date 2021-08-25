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

#ifndef XENOLITH_GL_VK_XLVKSWAPCHAIN_H_
#define XENOLITH_GL_VK_XLVKSWAPCHAIN_H_

#include "XLVk.h"
#include "XLVkImageAttachment.h"

namespace stappler::xenolith::vk {

class Device;
class ImageView;
class Framebuffer;
class Fence;
class RenderPassImpl;

class SwapchainFramebuffer : public Ref {
public:

protected:
	Vector<Rc<ImageView>> attachments;
	Rc<Framebuffer> framebuffer;
};

class Swapchain : public Ref {
public:
	virtual ~Swapchain();

	bool init(Device &);

	bool recreateSwapChain(Device &, const Rc<gl::Loop> &, thread::TaskQueue &queue, bool resize);
	bool createSwapChain(Device &, const Rc<gl::Loop> &, thread::TaskQueue &queue, SurfaceInfo &&, gl::PresentMode);
	void cleanupSwapChain(Device &);
	void invalidate(Device &);

	gl::PresentMode getPresentMode() const { return _presentMode; }
	VkSwapchainKHR getSwapchain() const { return _swapChain; }

	bool isBestPresentMode() const;

	const Rc<gl::RenderQueue> getDefaultRenderQueue() const { return _defaultRenderQueue; }

protected:
	void buildAttachments(Device &device, gl::RenderQueue *, gl::RenderPassData *, const Vector<VkImage> &);
	void updateAttachment(Device &device, const Rc<gl::Attachment> &);
	void updateFramebuffer(Device &device, gl::RenderPassData *);

	// returns <best, fast>
	Pair<gl::PresentMode, gl::PresentMode> getPresentModes(const SurfaceInfo &) const;

	Rc<gl::RenderQueue> _defaultRenderQueue;
	VkSurfaceFormatKHR _format;
	gl::PresentMode _presentMode = gl::PresentMode::Fifo;
	gl::PresentMode _bestPresentMode = gl::PresentMode::Fifo;
	gl::PresentMode _fastPresentMode = gl::PresentMode::Fifo;
	SurfaceInfo _info;
	VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
	VkSwapchainKHR _oldSwapChain = VK_NULL_HANDLE;
	// Vector<VkImage> _swapChainImages;
	// Vector<Rc<ImageView>> _swapChainImageViews;
	// Vector<Rc<Framebuffer>> _swapChainFramebuffers;
};

}

#endif /* XENOLITH_GL_VK_XLVKSWAPCHAIN_H_ */
