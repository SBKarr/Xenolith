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

Rc<RenderPassHandle> RenderPass::makeFrameHandle(const FrameQueue &handle) {
	return Rc<RenderPassHandle>::create(*this, handle);
}

bool RenderPass::acquireForFrame(gl::FrameQueue &frame, Function<void(bool)> &&onAcquired) {
	if (_owner) {
		if (_next.queue) {
			_next.acquired(false);
		}
		_next = FrameQueueWaiter{
			&frame,
			move(onAcquired)
		};
		return false;
	} else {
		_owner = &frame;
		return true;
	}
}

bool RenderPass::releaseForFrame(gl::FrameQueue &frame) {
	if (_owner.get() == &frame) {
		if (_next.queue) {
			_owner = move(_next.queue);
			_next.acquired(true);
			_next.queue = nullptr;
			_next.acquired = nullptr;
		} else {
			_owner = nullptr;
		}
		return true;
	} else if (_next.queue.get() == &frame) {
		auto tmp = move(_next.acquired);
		_next.queue = nullptr;
		_next.acquired = nullptr;
		tmp(false);
		return true;
	}
	return false;
}

Extent2 RenderPass::getSizeForFrame(const FrameQueue &queue) const {
	if (_frameSizeCallback) {
		return _frameSizeCallback(queue);
	} else {
		return queue.getExtent();
	}
}

void RenderPass::prepare(Device &device) {

}

RenderPassHandle::~RenderPassHandle() { }

bool RenderPassHandle::init(RenderPass &pass, const FrameQueue &queue) {
	_renderPass = &pass;
	_data = pass.getData();
	return true;
}

void RenderPassHandle::setQueueData(FrameQueueRenderPassData &data) {
	_queueData = &data;
}

StringView RenderPassHandle::getName() const {
	return _data->key;
}

bool RenderPassHandle::isAvailable(const FrameQueue &handle) const {
	return true;
}

bool RenderPassHandle::isSubmitted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Submitted);
}

bool RenderPassHandle::isCompleted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Complete);
}

bool RenderPassHandle::prepare(FrameQueue &, Function<void(bool)> &&) {
	return true;
}

void RenderPassHandle::submit(FrameQueue &, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {

}

void RenderPassHandle::finalize(FrameQueue &, bool successful) {

}

AttachmentHandle *RenderPassHandle::getAttachmentHandle(const Attachment *a) const {
	auto it = _queueData->attachmentMap.find(a);
	if (it != _queueData->attachmentMap.end()) {
		return it->second->handle.get();
	}
	return nullptr;
}

}
