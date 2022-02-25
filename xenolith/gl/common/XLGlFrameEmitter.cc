/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLGlFrameEmitter.h"
#include "XLGlFrameHandle.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::gl {

FrameRequest::~FrameRequest() {
	if (_queue) {
		_queue->endFrame(*this);
	}
}

bool FrameRequest::init(const Rc<RenderQueue> &q) {
	_queue = q;
	_queue->beginFrame(*this);
	return true;
}

bool FrameRequest::init(const Rc<RenderQueue> &q, const Rc<FrameEmitter> &emitter, Extent2 extent) {
	if (init(q)) {
		_emitter = emitter;
		_extent = extent;
		return true;
	}
	return false;
}

void FrameRequest::setCacheInfo(const Rc<FrameEmitter> &e, const Rc<FrameCacheStorage> &c, bool readyForSubmit) {
	_emitter = e;
	_cache = c;
	_readyForSubmit = readyForSubmit;
}

void FrameRequest::addInput(const Attachment *a, Rc<AttachmentInputData> &&data) {
	_input.emplace(a, move(data));
}

void FrameRequest::acquireInput(Map<const Attachment *, Rc<AttachmentInputData>> &target) {
	if (!_input.empty()) {
		target = move(_input);
	}
	_input.clear();
}

bool FrameRequest::onOutputReady(gl::Loop &loop, FrameQueueAttachmentData &data) const {
	if (data.handle->getAttachment() == _swapchainAttachment) {
		auto v = Rc<Swapchain::PresentTask>::alloc(_cache, (const gl::ImageAttachment *)data.handle->getAttachment().get(), data.image);
		if (_swapchain->present(loop, v)) {
			return true;
		}
	}

	auto it = _output.find(data.handle->getAttachment());
	if (it != _output.end()) {
		if (it->second(_cache, data)) {
			return true;
		}
		return false;
	}
	return false;
}

void FrameRequest::finalize() {
	if (_cache) {
		_cache = nullptr;
	}
	if (_emitter) {
		_emitter = nullptr;
	}
}

bool FrameRequest::bindSwapchain(const Rc<Swapchain> &swapchain) {
	for (auto &it : _queue->getOutputAttachments()) {
		if (it->getType() == AttachmentType::Image && it->isCompatible(swapchain->getSwapchainImageInfo())) {
			_swapchainAttachment = it;
			_swapchain = swapchain;
			return true;
		}
	}
	return false;
}

bool FrameRequest::bindSwapchain(const Attachment *a, const Rc<Swapchain> &swapchain) {
	if (a->isCompatible(swapchain->getSwapchainImageInfo())) {
		_swapchainAttachment = a;
		_swapchain = swapchain;
		return true;
	}
	return false;
}

FrameEmitter::~FrameEmitter() { }

bool FrameEmitter::init(const Rc<Loop> &loop, uint64_t frameInterval) {
	_frameInterval = frameInterval;
	_loop = loop;
	return true;
}

void FrameEmitter::invalidate() {
	_valid = true;
	for (auto &it : _frames) {
		it->invalidate();
	}
	_frames.clear();

	auto cache = move(_frameCache);
	_frameCache.clear();

	for (auto &it : cache) {
		it.second->invalidate();
	}
}

void FrameEmitter::setFrameSubmitted(gl::FrameHandle &frame) {
	XL_FRAME_EMITTER_LOG("FrameTime:        ", _frame, "   ", platform::device::_clock() - _frame, " mks");

	auto it = _frames.begin();
	while (it != _frames.end()) {
		if ((*it) == &frame) {
			it = _frames.erase(it);
		} else {
			++ it;
		}
	}

	XL_PROFILE_BEGIN(success, "FrameEmitter::setFrameSubmitted", "success", 500);
	XL_PROFILE_BEGIN(onFrameSubmitted, "FrameEmitter::setFrameSubmitted", "onFrameSubmitted", 500);
	onFrameSubmitted(frame);
	XL_PROFILE_END(onFrameSubmitted)

	XL_PROFILE_BEGIN(setReadyForSubmit, "FrameEmitter::setFrameSubmitted", "setReadyForSubmit", 500);
	for (auto &it : _frames) {
		if (!it->isReadyForSubmit()) {
			it->setReadyForSubmit(true);
			break;
		}
	}
	XL_PROFILE_END(setReadyForSubmit)

	++ _submitted;
	XL_PROFILE_BEGIN(onFrameRequest, "FrameEmitter::setFrameSubmitted", "onFrameRequest", 500);
	onFrameRequest(false);
	XL_PROFILE_END(onFrameRequest)
	XL_PROFILE_END(success)
}

bool FrameEmitter::isFrameValid(const gl::FrameHandle &frame) {
	if (_valid && frame.getGen() == _gen && std::find(_frames.begin(), _frames.end(), &frame) != _frames.end()) {
		return true;
	}
	return false;
}

void FrameEmitter::removeCacheStorage(const FrameCacheStorage *storage) {
	auto it = _frameCache.find(storage->getQueue());
	if (it != _frameCache.end()) {
		_frameCache.erase(it);
	}
}

void FrameEmitter::acquireNextFrame() { }

void FrameEmitter::dropFrameTimeout() {
	_loop->performOnThread([this] {
		if (!_frameTimeoutPassed) {
			++ _order; // increment timeout timeline
			onFrameTimeout(_order);
		}
	}, this, true);
}

void FrameEmitter::onFrameEmitted(gl::FrameHandle &) { }

void FrameEmitter::onFrameSubmitted(gl::FrameHandle &) { }

void FrameEmitter::onFrameComplete(gl::FrameHandle &) { }

void FrameEmitter::onFrameTimeout(uint64_t order) {
	if (order == _order) {
		_frameTimeoutPassed = true;
		onFrameRequest(true);
	}
}

void FrameEmitter::onFrameRequest(bool timeout) {
	if (canStartFrame()) {
		auto next = platform::device::_clock();
		if (_frame) {
			XL_FRAME_EMITTER_LOG(timeout ? "FrameRequest [T]: " : "FrameRequest [S]: ", _frame, "   ",
					next - _frame, " mks");
		}
		_frame = next;

		if (_nextFrameRequest) {
			scheduleFrameTimeout();
			submitNextFrame(move(_nextFrameRequest));
		} else if (!_nextFrameAcquired) {
			_nextFrameAcquired = true;
			scheduleFrameTimeout();
			acquireNextFrame();
		}
	}
}

Rc<FrameHandle> FrameEmitter::makeFrame(Rc<FrameRequest> &&req, bool readyForSubmit) {
	if (!_valid) {
		return nullptr;
	}

	auto queue = req->getQueue().get();

	auto cacheIt = _frameCache.find(queue);
	if (cacheIt == _frameCache.end()) {
		cacheIt = _frameCache.emplace(queue, Rc<FrameCacheStorage>::create(_loop->getDevice(), this, queue)).first;
	}

	req->setCacheInfo(this, cacheIt->second, readyForSubmit);

	return _loop->getDevice()->makeFrame(*_loop, move(req), _gen);
}

bool FrameEmitter::canStartFrame() const {
	if (!_valid || !_frameTimeoutPassed) {
		return false;
	}

	if (_frames.empty()) {
		return true;
	}

	for (auto &it : _frames) {
		if (!it->isSubmitted()) {
			return false;
		}
	}

	return true;
}

void FrameEmitter::scheduleNextFrame(Rc<FrameRequest> &&req) {
	_nextFrameRequest = move(req);
}

void FrameEmitter::scheduleFrameTimeout() {
	if (_valid && _frameInterval && _frameTimeoutPassed) {
		_frameTimeoutPassed = false;
		++ _order;
		auto t = platform::device::_clock();
		_loop->schedule([this, t, guard = Rc<FrameEmitter>(this), idx = _order] (Loop::Context &ctx) {
			XL_FRAME_EMITTER_LOG("TimeoutPassed:    ", _frame, "   ", platform::device::_clock() - _frame, " (",
					platform::device::_clock() - t, ") mks");
			guard->onFrameTimeout(idx);
			return true; // end spinning
		}, _frameInterval, "FrameEmitter::scheduleFrameTimeout");
	}
}

Rc<gl::FrameHandle> FrameEmitter::submitNextFrame(Rc<FrameRequest> &&req) {
	if (!_valid) {
		return nullptr;
	}

	auto frame = makeFrame(move(req), _frames.empty());
	_nextFrameRequest = nullptr;
	if (frame && frame->isValidFlag()) {
		frame->setCompleteCallback([this] (FrameHandle &frame) {
			onFrameComplete(frame);
		});

		auto t = platform::device::_clock();
		_loop->performOnThread([this, t, frame] () mutable {
			XL_FRAME_EMITTER_LOG("Sync: ", platform::device::_clock() - t, " mks");
			XL_FRAME_EMITTER_LOG("SubmitNextFrame:  ", _frame, "   ", platform::device::_clock() - _frame, " mks");

			_nextFrameAcquired = false;
			onFrameEmitted(*frame);
			frame->update(true);
			if (frame->isValidFlag()) {
				_frames.push_back(move(frame));
			}
		}, this, true);
		return frame;
	}
	return nullptr;
}

}
