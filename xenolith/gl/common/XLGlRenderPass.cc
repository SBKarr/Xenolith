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

RenderPass::~RenderPass() { }

bool RenderPass::init(StringView name, RenderPassType type, RenderOrdering order, size_t subpassCount) {
	_name = name.str();
	_type = type;
	_ordering = order;
	_subpassCount = subpassCount;
	return true;
}

void RenderPass::invalidate() {

}

Rc<RenderPassHandle> RenderPass::makeFrameHandle(RenderPassData *data, const FrameHandle &handle) {
	return Rc<RenderPassHandle>::create(*this, data, handle);
}

bool RenderPass::acquireForFrame(gl::FrameHandle &frame) {
	if (_owner) {
		if (_next) {
			_next->invalidate();
		}
		_next = &frame;
		return false;
	} else {
		_owner = &frame;
		return true;
	}
}

bool RenderPass::releaseForFrame(gl::FrameHandle &frame) {
	if (_owner.get() == &frame) {
		if (_next) {
			_owner = move(_next);
			_next = nullptr;
			_owner->getLoop()->pushEvent(Loop::EventName::FrameUpdate, _owner);
		} else {
			_owner = nullptr;
		}
		return true;
	} else if (_next.get() == &frame) {
		_next = nullptr;
		return true;
	}
	return false;
}

void RenderPass::prepare(Device &) {

}

RenderPassHandle::~RenderPassHandle() { }

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
				addRequiredAttachment(a->getAttachment(), it);
			}
		}
	}

	for (auto &a : _attachments) {
		auto &desc = a.first->getDescriptors();
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
		if (!it.second->isReady()) {
			ready = false;
		}
	}

	return ready;
}

bool RenderPassHandle::isAvailable(const FrameHandle &handle) const {
	return _isAsync || _renderPass->getOwner() == &handle;
}

bool RenderPassHandle::prepare(FrameHandle &) {
	return true;
}

void RenderPassHandle::submit(FrameHandle &, Function<void(const Rc<RenderPass> &)> &&) {

}

AttachmentHandle *RenderPassHandle::getAttachmentHandle(const Attachment *a) const {
	auto it = _attachments.find(a);
	if (it != _attachments.end()) {
		return it->second;
	}
	return nullptr;
}

void RenderPassHandle::addRequiredAttachment(const Attachment *a, const Rc<AttachmentHandle> &h) {
	_attachments.emplace(a, h);
}

}
