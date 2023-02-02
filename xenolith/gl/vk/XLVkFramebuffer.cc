/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLVkFramebuffer.h"

namespace stappler::xenolith::vk {

bool Framebuffer::init(Device &dev, RenderPassImpl *renderPass, SpanView<Rc<gl::ImageView>> imageViews, Extent2 extent) {
	Vector<VkImageView> views; views.reserve(imageViews.size());
	_viewIds.reserve(imageViews.size());
	_imageViews.reserve(imageViews.size());
	_renderPass = renderPass;

	for (auto &it : imageViews) {
		views.emplace_back(((ImageView *)it.get())->getImageView());
		_viewIds.emplace_back(it->getIndex());
		_imageViews.emplace_back(it);

		auto e = it->getExtent();
		if (e.width != extent.width || e.height != extent.height) {
			return false;
		}
	}

	VkFramebufferCreateInfo framebufferInfo { };
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass->getRenderPass();
	framebufferInfo.attachmentCount = views.size();
	framebufferInfo.pAttachments = views.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	if (dev.getTable()->vkCreateFramebuffer(dev.getDevice(), &framebufferInfo, nullptr, &_framebuffer) == VK_SUCCESS) {
		_extent = extent;
		return gl::Framebuffer::init(dev, [] (gl::Device *dev, gl::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyFramebuffer(d->getDevice(), (VkFramebuffer)ptr.get(), nullptr);
		}, gl::ObjectType::Framebuffer, ObjectHandle(_framebuffer));
	}
	return false;
}

}
