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

#include "XLRenderQueueQueue.h"
#include "XLRenderQueueFrameEmitter.h"
#include "XLRenderQueueFrameHandle.h"
#include "XLGlView.h"

namespace stappler::xenolith::renderqueue {

FrameRequest::~FrameRequest() {
	if (_queue) {
		_queue->endFrame(*this);
	}
}

bool FrameRequest::init(const Rc<FrameEmitter> &emitter, Rc<ImageStorage> &&target, float density) {
	auto e = target->getInfo().extent;
	_emitter = emitter;
	_extent = Extent2(e.width, e.height);
	_readyForSubmit = false;
	_renderTarget = move(target);
	_density = density;
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q) {
	_queue = q;
	_queue->beginFrame(*this);
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q, const Rc<FrameEmitter> &emitter, Extent2 extent, float density) {
	if (init(q)) {
		_emitter = emitter;
		_extent = extent;
		_density = density;
		_readyForSubmit = emitter->isReadyForSubmit();
		return true;
	}
	return false;
}

void FrameRequest::addSignalDependency(Rc<DependencyEvent> &&dep) {
	if (dep) {
		if (dep->submitted.exchange(true)) {
			++ dep->signaled;
		}
		_signalDependencies.emplace_back(move(dep));
	}
}

void FrameRequest::addSignalDependencies(Vector<Rc<DependencyEvent>> &&deps) {
	for (auto &dep : deps) {
		if (dep->submitted.exchange(true)) {
			++ dep->signaled;
		}
	}
	if (_signalDependencies.empty()) {
		_signalDependencies = move(deps);
	} else {
		for (auto &it : deps) {
			_signalDependencies.emplace_back(move(it));
		}
	}
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

void FrameRequest::setQueue(const Rc<Queue> &q) {
	if (_queue != q) {
		if (_queue) {
			_queue->endFrame(*this);
		}
		_queue = q;
		if (_queue) {
			_queue->beginFrame(*this);
		}
	}
}

void FrameRequest::setOutput(const Attachment *a, Function<bool(const FrameAttachmentData &, bool)> &&cb) {
	_output.emplace(a, move(cb));
}

bool FrameRequest::onOutputReady(gl::Loop &loop, FrameAttachmentData &data) const {
	auto it = _output.find(data.handle->getAttachment());
	if (it != _output.end()) {
		if (it->second(data, true)) {
			return true;
		}
		return false;
	}

	if (data.handle->getAttachment() == _swapchainAttachment && data.image) {
		if (_swapchain->present(move(data.image))) {
			return true;
		}
	}

	return false;
}

void FrameRequest::onOutputInvalidated(gl::Loop &loop, FrameAttachmentData &data) const {
	auto it = _output.find(data.handle->getAttachment());
	if (it != _output.end()) {
		if (it->second(data, false)) {
			return;
		}
	}

	if (data.handle->getAttachment() == _swapchainAttachment && data.image) {
		_swapchain->invalidateTarget(move(data.image));
	}
}

void FrameRequest::finalize(gl::Loop &loop, bool success) {
	if (!success) {
		if (_swapchain && _renderTarget) {
			_swapchain->invalidateTarget(move(_renderTarget));
		}
		_swapchain = nullptr;
		_renderTarget = nullptr;
	}
	if (_emitter) {
		_emitter = nullptr;
	}

	loop.signalDependencies(_signalDependencies, success);
}

bool FrameRequest::bindSwapchainCallback(Function<bool(FrameAttachmentData &, bool)> &&cb) {
	for (auto &it : _queue->getOutputAttachments()) {
		if (it->getType() == AttachmentType::Image) {
			_output.emplace(it, move(cb));
			return true;
		}
	}
	return false;
}

bool FrameRequest::bindSwapchain(const Rc<gl::View> &swapchain) {
	_density = swapchain->getDensity();
	for (auto &it : _queue->getOutputAttachments()) {
		if (it->getType() == AttachmentType::Image && it->isCompatible(swapchain->getSwapchainImageInfo())) {
			_swapchainAttachment = it;
			_swapchain = swapchain;
			_swapchainHandle = _swapchain->getSwapchainHandle();
			return true;
		}
	}
	return false;
}

bool FrameRequest::bindSwapchain(const Attachment *a, const Rc<gl::View> &swapchain) {
	if (a->isCompatible(swapchain->getSwapchainImageInfo())) {
		_swapchainAttachment = a;
		_swapchain = swapchain;
		_swapchainHandle = _swapchain->getSwapchainHandle();
		return true;
	}
	return false;
}

bool FrameRequest::isSwapchainAttachment(const Attachment *a) const {
	return _swapchainAttachment == a;
}

Set<Rc<Queue>> FrameRequest::getQueueList() const {
	return Set<Rc<Queue>>{_queue};
}

FrameEmitter::~FrameEmitter() { }

bool FrameEmitter::init(const Rc<gl::Loop> &loop, uint64_t frameInterval) {
	_frameInterval = frameInterval;
	_loop = loop;

	_avgFrameTime.reset(0);
	_avgFrameTimeValue = 0;

	return true;
}

void FrameEmitter::invalidate() {
	_valid = false;
	auto frames = _frames;
	for (auto &it : frames) {
		it->invalidate();
	}
	_frames.clear();
}

void FrameEmitter::setFrameSubmitted(FrameHandle &frame) {
	if (!_loop->isOnGlThread()) {
		return;
	}

	XL_FRAME_EMITTER_LOG("FrameTime:        ", _frame.load(), "   ", platform::device::_clock() - _frame.load(), " mks");

	auto it = _frames.begin();
	while (it != _frames.end()) {
		if ((*it) == &frame) {
			if (frame.isValid()) {
				_framesPending.emplace_back(&frame);
			}
			it = _frames.erase(it);
		} else {
			++ it;
		}
	}

	XL_PROFILE_BEGIN(success, "FrameEmitter::setFrameSubmitted", "success", 500);
	XL_PROFILE_BEGIN(onFrameSubmitted, "FrameEmitter::setFrameSubmitted", "onFrameSubmitted", 500);
	onFrameSubmitted(frame);
	XL_PROFILE_END(onFrameSubmitted)

	++ _submitted;
	XL_PROFILE_BEGIN(onFrameRequest, "FrameEmitter::setFrameSubmitted", "onFrameRequest", 500);
	if (!_onDemand) {
		onFrameRequest(false);
	}
	XL_PROFILE_END(onFrameRequest)
	XL_PROFILE_END(success)
}

bool FrameEmitter::isFrameValid(const FrameHandle &frame) {
	if (_valid && frame.getGen() == _gen && std::find(_frames.begin(), _frames.end(), &frame) != _frames.end()) {
		return true;
	}
	return false;
}

void FrameEmitter::acquireNextFrame() { }

void FrameEmitter::dropFrameTimeout() {
	_loop->performOnGlThread([this] {
		if (!_frameTimeoutPassed) {
			++ _order; // increment timeout timeline
			onFrameTimeout(_order);
		}
	}, this, true);
}

void FrameEmitter::dropFrames() {
	if (!_loop->isOnGlThread()) {
		return;
	}

	for (auto &it : _frames) {
		it->invalidate();
	}
	_frames.clear();
	_framesPending.clear();
}

uint64_t FrameEmitter::getLastFrameTime() const {
	return _lastFrameTime;
}
uint64_t FrameEmitter::getAvgFrameTime() const {
	return _avgFrameTimeValue;
}

bool FrameEmitter::isReadyForSubmit() const {
	return _frames.empty() && _framesPending.empty();
}

void FrameEmitter::onFrameEmitted(FrameHandle &) { }

void FrameEmitter::onFrameSubmitted(FrameHandle &) { }

void FrameEmitter::onFrameComplete(FrameHandle &frame) {
	if (!_loop->isOnGlThread()) {
		return;
	}

	_lastFrameTime = frame.getTimeEnd() - frame.getTimeStart();
	_avgFrameTime.addValue(frame.getTimeEnd() - frame.getTimeStart());
	_avgFrameTimeValue = _avgFrameTime.getAverage(true);

	auto it = _framesPending.begin();
	while (it != _framesPending.end()) {
		if ((*it) == &frame) {
			it = _framesPending.erase(it);
		} else {
			++ it;
		}
	}

	if (_framesPending.size() <= 1 && _frames.empty() && !_onDemand) {
		onFrameRequest(false);
	}

	if (_framesPending.empty()) {
		for (auto &it : _frames) {
			if (!it->isReadyForSubmit()) {
				it->setReadyForSubmit(true);
				break;
			}
		}
	}
}

void FrameEmitter::onFrameTimeout(uint64_t order) {
	if (order == _order) {
		_frameTimeoutPassed = true;
		onFrameRequest(true);
	}
}

void FrameEmitter::onFrameRequest(bool timeout) {
	if (canStartFrame()) {
		auto next = platform::device::_clock();

		if (_nextFrameRequest) {
			scheduleFrameTimeout();
			submitNextFrame(move(_nextFrameRequest));
		} else if (!_nextFrameAcquired) {
			if (_frame.load()) {
				XL_FRAME_EMITTER_LOG(timeout ? "FrameRequest [T]: " : "FrameRequest [S]: ", _frame.load(), "   ",
						next - _frame.load(), " mks");
			}
			_frame = next;
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

	req->setReadyForSubmit(readyForSubmit);
	auto frame = _loop->makeFrame(move(req), _gen);

	enableCacheAttachments(frame);

	return frame;
}

bool FrameEmitter::canStartFrame() const {
	if (!_valid || !_frameTimeoutPassed) {
		return false;
	}

	if (_frames.empty()) {
		return _framesPending.size() <= 1;
	}

	for (auto &it : _frames) {
		if (!it->isSubmitted()) {
			return false;
		}
	}

	return _framesPending.size() <= 1;
}

void FrameEmitter::scheduleNextFrame(Rc<FrameRequest> &&req) {
	_nextFrameRequest = move(req);
}

void FrameEmitter::scheduleFrameTimeout() {
	if (_valid && _frameInterval && _frameTimeoutPassed && !_onDemand) {
		_frameTimeoutPassed = false;
		++ _order;
		auto t = platform::device::_clock(platform::device::ClockType::Monotonic);
		_loop->schedule([this, t, guard = Rc<FrameEmitter>(this), idx = _order] (gl::Loop &ctx) {
			XL_FRAME_EMITTER_LOG("TimeoutPassed:    ", _frame.load(), "   ", platform::device::_clock() - _frame.load(), " (",
					platform::device::_clock(platform::device::ClockType::Monotonic) - t, ") mks");
			guard->onFrameTimeout(idx);
			return true; // end spinning
		}, _frameInterval - config::FrameIntervalSafeOffset, "FrameEmitter::scheduleFrameTimeout");
	}
}

Rc<FrameRequest> FrameEmitter::makeRequest(Rc<ImageStorage> &&storage, float density) {
	_frame = platform::device::_clock();
	return Rc<FrameRequest>::create(this, move(storage), density);
}

Rc<FrameHandle> FrameEmitter::submitNextFrame(Rc<FrameRequest> &&req) {
	if (!_valid) {
		return nullptr;
	}

	bool readyForSubmit = _frames.empty() && _framesPending.empty();
	auto frame = makeFrame(move(req), readyForSubmit);
	_nextFrameRequest = nullptr;
	if (frame && frame->isValidFlag()) {
		auto now = platform::device::_clock();
		_lastSubmit = now;

		frame->setCompleteCallback([this] (FrameHandle &frame) {
			onFrameComplete(frame);
		});

		XL_FRAME_EMITTER_LOG("SubmitNextFrame:  ", _frame.load(), "   ", platform::device::_clock() - _frame.load(), " mks ", readyForSubmit);

		_nextFrameAcquired = false;
		onFrameEmitted(*frame);
		frame->update(true);
		if (frame->isValidFlag()) {
			if (_frames.empty() && _framesPending.empty() && !frame->isReadyForSubmit()) {
				_frames.push_back(frame);
				frame->setReadyForSubmit(true);
			} else {
				_frames.push_back(frame);
			}
		}
		return frame;
	}
	return nullptr;
}

void FrameEmitter::enableCacheAttachments(const Rc<FrameHandle> &req) {
	auto &queues = req->getFrameQueues();

	Set<Rc<Queue>> list;
	for (auto &it : queues) {
		list.emplace(it->getRenderQueue());
	}

	Extent2 targetExtent = req->getExtent();

	if (_cacheRenderQueue != list || targetExtent != _cacheExtent) {
		Set<gl::ImageInfoData> images;
		for (auto &it : queues) {
			for (auto &a : it->getRenderQueue()->getAttachments()) {
				if (a->getType() == AttachmentType::Image) {
					auto img = (ImageAttachment *)a.get();
					auto data = img->getInfo();
					data.extent = img->getSizeForFrame(*it);
					images.emplace(data);

					// for possible transient attachment add transient version of image
					if (a->isTransient()) {
						data.usage |=  gl::ImageUsage::TransientAttachment;
						images.emplace(data);
					}
				}
			}
		}

		_cacheRenderQueue = list;
		_cacheExtent = targetExtent;

		for (auto &it : images) {
			auto iit = _cacheImages.find(it);
			if (iit != _cacheImages.end()) {
				_cacheImages.erase(it);
			} else {
				_loop->getFrameCache()->addImage(it);
			}
		}

		for (auto &it : _cacheImages) {
			_loop->getFrameCache()->removeImage(it);
		}

		_cacheImages = move(images);

		_loop->getFrameCache()->removeUnreachableFramebuffers();
	}
}

}
