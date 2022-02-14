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

#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

class OutputImageAttachmentDescriptor : public gl::ImageAttachmentDescriptor {
public:
	virtual ~OutputImageAttachmentDescriptor() { }

	virtual void clear() override {
		_imageView = nullptr;
	}

	const Rc<ImageView> &getImageView() const { return _imageView; }
	virtual void setImageView(const Rc<ImageView> &v) { _imageView = v; }

protected:
	Rc<ImageView> _imageView;
};

class OutputImageAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~OutputImageAttachmentHandle() { }

	virtual bool setup(gl::FrameQueue &, Function<void(bool)> &&) override { return true; }
};

OutputImageAttachment::~OutputImageAttachment() { }

void OutputImageAttachment::clear() {
	gl::ImageAttachment::clear();
	_images.clear();
}

Rc<gl::AttachmentHandle> OutputImageAttachment::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<OutputImageAttachmentHandle>::create(this, handle);
}

Rc<gl::AttachmentDescriptor> OutputImageAttachment::makeDescriptor(gl::RenderPassData *pass) {
	return Rc<OutputImageAttachmentDescriptor>::create(pass, this);
}

ImageAttachmentHandle::~ImageAttachmentHandle() {

}

}
