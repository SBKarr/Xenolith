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

#include "XLGlRenderPass.h"
#include "XLGlAttachment.h"

namespace stappler::xenolith::gl {

bool RenderPass::init(StringView name, RenderOrdering order, size_t subpassCount) {
	_name = name.str();
	_ordering = order;
	_subpassCount = subpassCount;
	return true;
}

void RenderPass::invalidate() {

}

Rc<RenderPassHandle> RenderPass::makeFrameHandle(RenderPassData *data, const FrameHandle &handle) {
	return Rc<RenderPassHandle>::create(*this, data, handle);
}

bool RenderPassHandle::init(RenderPass &pass, RenderPassData *data, const FrameHandle &frame) {
	_renderPass = &pass;
	_data = data;
	return true;
}

StringView RenderPassHandle::getName() const {
	return _data->key;
}

void RenderPassHandle::buildRequirements(const FrameHandle &frame, const Vector<Rc<RenderPassHandle>> &passes,
		const Vector<Rc<AttachmentHandle>> &attachments) {
	for (auto &it : attachments) {
		for (auto &a : _data->descriptors) {
			if (it->getAttachment() == a->getAttachment()) {
				_attachments.emplace_back(it);
			}
		}
	}

	for (auto &a : _attachments) {
		auto &desc = a->getAttachment()->getDescriptors();
		auto it = desc.begin();
		while (it != desc.end() && (*it)->getRenderPass() != _data) {
			for (auto &pass : passes) {
				if (pass->getData() == (*it)->getRenderPass()) {
					_requiredPasses.emplace_back(pass);
				}
			}
		}
	}
}

bool RenderPassHandle::isReady() const {
	bool ready = true;
	for (auto &it : _requiredPasses) {
		if (!it->isSubmitted()) {
			ready = false;
		}
	}

	for (auto &it : _attachments) {
		if (!it->isReady()) {
			ready = false;
		}
	}

	return ready;
}

bool RenderPassHandle::run(FrameHandle &) {
	return true;
}

void RenderPassHandle::submit(FrameHandle &) {

}

}
