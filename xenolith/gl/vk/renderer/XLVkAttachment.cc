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

#include "XLVkAttachment.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

auto ImageAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<ImageAttachmentHandle>::create(this, handle);
}

bool ImageAttachmentHandle::writeDescriptor(const QueuePassHandle &queue, DescriptorImageInfo &info) {
	auto &image = _queueData->image;
	auto desc = (renderqueue::ImageAttachmentDescriptor *)info.descriptor->descriptor;
	auto viewInfo = gl::ImageViewInfo(*desc, image->getInfo());
	if (auto view = image->getView(viewInfo)) {
		info.layout = VkImageLayout(desc->getDescriptorLayout());
		info.imageView = (ImageView *)view.get();
		return true;
	}

	return false;
}

bool ImageAttachmentHandle::isDescriptorDirty(const PassHandle &, const PipelineDescriptor &d, uint32_t, bool isExternal) const {
	return getImage();
}

}
