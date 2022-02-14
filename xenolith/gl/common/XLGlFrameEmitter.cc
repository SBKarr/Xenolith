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

bool FrameCacheStorage::init(Device *dev, const FrameEmitter *e, const RenderQueue *q) {
	device = dev;
	emitter = e;
	queue = q;

	queue->addCacheStorage(this);

	for (auto &it : queue->getPasses()) {
		passes.emplace(it, FrameCacheRenderPass{it});
	}
	for (auto &it : queue->getAttachments()) {
		if (it->getType() == AttachmentType::Image) {
			auto imgAttachment = (ImageAttachment *)it.get();
			images.emplace(imgAttachment, FrameCacheImageAttachment{imgAttachment});
		}
	}
	return true;
}

void FrameCacheStorage::invalidate() {
	_invalidateMutex.lock();
	passes.clear();
	images.clear();
	if (queue) {
		queue->removeCacheStorage(this);
		queue = nullptr;
	}
	if (emitter) {
		const_cast<FrameEmitter *>(emitter)->removeCacheStorage(this);
		emitter = nullptr;
	}
	_invalidateMutex.unlock();
}

void FrameCacheStorage::reset(const RenderPassData *p, Extent2 e) {
	auto it = passes.find(p);
	if (it == passes.end()) {
		return;
	}

	if (it->second.extent == e) {
		return;
	}

	it->second.extent = e;
	it->second.framebuffers.clear();
}

Rc<Framebuffer> FrameCacheStorage::acquireFramebuffer(const Loop &, const RenderPassData *p, SpanView<Rc<ImageView>> views) {
	auto passIt = passes.find(p);
	if (passIt == passes.end()) {
		return nullptr;
	}

	Vector<uint64_t> ids; ids.reserve(views.size());
	for (auto &it : views) {
		ids.emplace_back(it->getIndex());
	}

	auto hash = Framebuffer::getViewHash(ids);

	auto range = passIt->second.framebuffers.equal_range(hash);
	auto it = range.first;
	while (it != range.second) {
		if ((*it).second->getViewIds() == ids) {
			auto ret = move((*it).second);
			passIt->second.framebuffers.erase(it);
			return ret;
		}
		++ it;
	}

	return device->makeFramebuffer(p, views, passIt->second.extent);
}

void FrameCacheStorage::releaseFramebuffer(const RenderPassData *p, Rc<Framebuffer> &&fb) {
	auto passIt = passes.find(p);
	if (passIt == passes.end()) {
		return;
	}

	if (fb->getExtent() != passIt->second.extent) {
		return;
	}

	auto hash = fb->getHash();
	passIt->second.framebuffers.emplace(hash, move(fb));
}

void FrameCacheStorage::reset(const ImageAttachment *a, Extent3 e) {
	auto imageIt = images.find(a);
	if (imageIt == images.end()) {
		return;
	}

	if (imageIt->second.extent == e) {
		return;
	}

	imageIt->second.images.clear();
	imageIt->second.extent = e;
}

Rc<ImageAttachmentObject> FrameCacheStorage::acquireImage(const Loop &, const ImageAttachment *a) {
	auto imageIt = images.find(a);
	if (imageIt == images.end()) {
		return nullptr;
	}

	if (!imageIt->second.images.empty()) {
		auto ret = move(imageIt->second.images.back());
		imageIt->second.images.pop_back();
		return ret;
	} else {
		return device->makeImage(a, imageIt->second.extent);
	}
}

void FrameCacheStorage::releaseImage(const ImageAttachment *a, Rc<ImageAttachmentObject> &&image) {
	auto imageIt = images.find(a);
	if (imageIt == images.end()) {
		return;
	}

	if (image->extent != imageIt->second.extent) {
		return;
	}

	imageIt->second.images.emplace_back(move(image));
}


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

void FrameRequest::finalize() {
	if (_cache) {
		_cache = nullptr;
	}
	if (_emitter) {
		_emitter = nullptr;
	}
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
	log::vtext("gl::FrameEmitter", "FrameTime:        ", _frame, "   ", platform::device::_clock() - _frame, " mks");

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
	auto it = _frameCache.find(storage->queue);
	if (it != _frameCache.end()) {
		_frameCache.erase(it);
	}
}

void FrameEmitter::acquireNextFrame() { }

void FrameEmitter::onFrameEmitted(gl::FrameHandle &) { }

void FrameEmitter::onFrameSubmitted(gl::FrameHandle &) { }

void FrameEmitter::onFrameComplete(gl::FrameHandle &) { }

void FrameEmitter::onFrameTimeout() {
	_frameTimeoutPassed = true;
	onFrameRequest(true);
}

void FrameEmitter::onFrameRequest(bool timeout) {
	if (canStartFrame()) {
		auto next = platform::device::_clock();
		if (_frame) {
			log::vtext("gl::FrameEmitter",
					timeout ? "FrameRequest [T]: " : "FrameRequest [S]: ", _frame, "   ",
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
		auto id = retain();
		auto t = platform::device::_clock();
		_loop->schedule([this, id, t] (Loop::Context &ctx) {
			auto dt = platform::device::_clock() - t;
			log::vtext("gl::FrameEmitter", "TimeoutPassed:    ", _frame, "   ", platform::device::_clock() - _frame, " (", dt, ") mks");
			onFrameTimeout();
			release(id);
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
			auto dt = platform::device::_clock() - t;
			log::vtext("gl::View", "Sync: ", dt, " mks");
			log::vtext("gl::FrameEmitter", "SubmitNextFrame:  ", _frame, "   ", platform::device::_clock() - _frame, " mks");

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
