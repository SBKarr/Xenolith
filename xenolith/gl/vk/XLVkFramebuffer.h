/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_GL_XLVKFRAMEBUFFER_H_
#define COMPONENTS_XENOLITH_GL_XLVKFRAMEBUFFER_H_

#include "XLVkDevice.h"
#include "XLGlObject.h"

namespace stappler::xenolith::vk {

class Image : public gl::Image {
public:
	virtual ~Image() { }

	bool init(Device &dev, VkImage, const gl::ImageInfo &);

	VkImage getImage() const { return _image; }

protected:
	VkImage _image = VK_NULL_HANDLE;
};

class ImageView : public gl::ImageView {
public:
	virtual ~ImageView() { }

	bool init(Device &dev, VkImage, VkFormat format);
	bool init(Device &dev, const gl::ImageAttachmentDescriptor &desc, Image *);

	VkImageView getImageView() const { return _imageView; }

protected:
	VkImageView _imageView = VK_NULL_HANDLE;
};

class Framebuffer : public gl::Framebuffer {
public:
	virtual ~Framebuffer() { }

	bool init(Device &dev, VkRenderPass renderPass, const Vector<VkImageView> &imageView, Extent2 extent);

	VkFramebuffer getFramebuffer() const { return _framebuffer; }
	const Extent2 &getExtent() const { return _extent; }

protected:
	Extent2 _extent;
	VkFramebuffer _framebuffer = VK_NULL_HANDLE;
};

}

#endif /* COMPONENTS_XENOLITH_GL_XLVKFRAMEBUFFER_H_ */
