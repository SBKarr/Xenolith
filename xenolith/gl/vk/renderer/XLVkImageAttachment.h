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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKIMAGEATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKIMAGEATTACHMENT_H_

#include "XLVkFramebuffer.h"
#include "XLVkSync.h"
#include "XLGlAttachment.h"

namespace stappler::xenolith::vk {

class Device;
class Semaphore;
class SwapchainAttachmentHandle;
class SwapchainSync;
class RenderPassHandle;

class ImageAttachment : public gl::ImageAttachment {
public:
	virtual ~ImageAttachment();

	virtual void clear() override;

	const Rc<Image> &getImage() const { return _image; }
	virtual void setImage(const Rc<Image> &);

protected:
	virtual Rc<gl::AttachmentDescriptor> makeDescriptor(gl::RenderPassData *) override;

	Rc<Image> _image;
};

class ImageAttachmentDescriptor : public gl::ImageAttachmentDescriptor {
public:
	virtual ~ImageAttachmentDescriptor();

	virtual void clear() override;

	const Rc<ImageView> &getImageView() const { return _imageView; }
	virtual void setImageView(const Rc<ImageView> &);

protected:
	Rc<ImageView> _imageView;
};

class SwapchainAttachment : public gl::SwapchainAttachment {
public:
	virtual ~SwapchainAttachment();

	virtual void clear() override;

	const Vector<Rc<Image>> &getImages() const { return _images; }
	virtual void setImages(Vector<Rc<Image>> &&);

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &);

protected:
	virtual Rc<gl::AttachmentDescriptor> makeDescriptor(gl::RenderPassData *) override;

	Vector<Rc<Image>> _images;
};

class SwapchainAttachmentDescriptor : public gl::SwapchainAttachmentDescriptor {
public:
	virtual ~SwapchainAttachmentDescriptor();

	virtual void clear() override;

	const Vector<Rc<ImageView>> &getImageViews() const { return _imageViews; }
	virtual void setImageViews(Vector<Rc<ImageView>> &&);

protected:
	Vector<Rc<ImageView>> _imageViews;
};

class ImageAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~ImageAttachmentHandle();

	virtual bool writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &, uint32_t, bool, VkDescriptorImageInfo &) { return false; }
};

class SwapchainAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~SwapchainAttachmentHandle();

	virtual bool isAvailable(const gl::FrameHandle &) const override;

	// returns true for immediate setup, false if setup job was scheduled
	virtual bool setup(gl::FrameHandle &) override;

	uint32_t getIndex() const { return _index; }
	const Rc<SwapchainSync> &getSync() const { return _sync; }
	Swapchain *getSwapchain() const { return _swapchain; }

	Rc<SwapchainSync> acquireSync();

protected:
	virtual bool acquire(gl::FrameHandle &);
	virtual void invalidate();

	uint32_t _index = maxOf<uint32_t>();

	Rc<SwapchainSync> _sync;
	Device * _device = nullptr;
	Swapchain *_swapchain = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKIMAGEATTACHMENT_H_ */
