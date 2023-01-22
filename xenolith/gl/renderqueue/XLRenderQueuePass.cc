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

#include "XLRenderQueueAttachment.h"
#include "XLRenderQueuePass.h"

namespace stappler::xenolith::renderqueue {

Pass::~Pass() { }

bool Pass::init(StringView name, PassType type, RenderOrdering order, size_t subpassCount) {
	_name = name.str<Interface>();
	_type = type;
	_ordering = order;
	_subpassCount = subpassCount;
	return true;
}

void Pass::invalidate() {

}

Rc<PassHandle> Pass::makeFrameHandle(const FrameQueue &handle) {
	return Rc<PassHandle>::create(*this, handle);
}

bool Pass::acquireForFrame(FrameQueue &frame, Function<void(bool)> &&onAcquired) {
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

bool Pass::releaseForFrame(FrameQueue &frame) {
	if (_owner == &frame) {
		if (_next.queue) {
			_owner = move(_next.queue);
			_next.acquired(true);
			_next.queue = nullptr;
			_next.acquired = nullptr;
		} else {
			_owner = nullptr;
		}
		return true;
	} else if (_next.queue == &frame) {
		auto tmp = move(_next.acquired);
		_next.queue = nullptr;
		_next.acquired = nullptr;
		tmp(false);
		return true;
	}
	return false;
}

Extent2 Pass::getSizeForFrame(const FrameQueue &queue) const {
	if (_frameSizeCallback) {
		return _frameSizeCallback(queue);
	} else {
		return queue.getExtent();
	}
}

const AttachmentDescriptor *Pass::getDescriptor(const Attachment *a) const {
	for (auto &it : _data->descriptors) {
		if (it->getAttachment() == a) {
			return it;
		}
	}
	return nullptr;
}

void Pass::prepare(gl::Device &device) {

}

PassHandle::~PassHandle() { }

bool PassHandle::init(Pass &pass, const FrameQueue &queue) {
	_renderPass = &pass;
	_data = pass.getData();
	return true;
}

void PassHandle::setQueueData(FramePassData &data) {
	_queueData = &data;
}

StringView PassHandle::getName() const {
	return _data->key;
}

bool PassHandle::isAvailable(const FrameQueue &handle) const {
	return true;
}

bool PassHandle::isSubmitted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Submitted);
}

bool PassHandle::isCompleted() const {
	return toInt(_queueData->state) >= toInt(FrameRenderPassState::Complete);
}

bool PassHandle::isFramebufferRequired() const {
	return _renderPass->getType() == PassType::Graphics;
}

bool PassHandle::prepare(FrameQueue &, Function<void(bool)> &&) {
	return true;
}

void PassHandle::submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {

}

void PassHandle::finalize(FrameQueue &, bool successful) {

}

AttachmentHandle *PassHandle::getAttachmentHandle(const Attachment *a) const {
	auto it = _queueData->attachmentMap.find(a);
	if (it != _queueData->attachmentMap.end()) {
		return it->second->handle.get();
	}
	return nullptr;
}

void PassHandle::autorelease(Ref *ref) {
	std::unique_lock lock(_autoreleaseMutex);
	_autorelease.emplace_back(ref);
}

}
