/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

FrameOutputBinding::FrameOutputBinding(const AttachmentData *a, CompleteCallback &&cb)
: callback(cb), attachment(a) { }

FrameOutputBinding::FrameOutputBinding(const AttachmentData *a, Rc<gl::View> &&view, CompleteCallback &&cb)
: view(view), handle(view->getSwapchainHandle()), callback(cb), attachment(a) { }

FrameOutputBinding::~FrameOutputBinding() { }

bool FrameOutputBinding::handleReady(FrameAttachmentData &data, bool success) {
	if (callback) {
		return callback(view, data, success);
	} else if (view && data.image) {
		if (success) {
			return view->present(move(data.image));
		} else {
			view->invalidateTarget(move(data.image));
			return true;
		}
	}
	return false;
}

FrameRequest::~FrameRequest() {
	if (_queue) {
		_queue->endFrame(*this);
	}
	_renderTargets.clear();
	_pool = nullptr;
}

bool FrameRequest::init(const Rc<FrameEmitter> &emitter, const gl::FrameContraints &constraints) {
	_pool = Rc<PoolRef>::alloc();
	_emitter = emitter;
	_constraints = constraints;
	_readyForSubmit = false;
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q) {
	_pool = Rc<PoolRef>::alloc();
	_queue = q;
	_queue->beginFrame(*this);
	return true;
}

bool FrameRequest::init(const Rc<Queue> &q, const gl::FrameContraints &constraints) {
	if (init(q)) {
		_constraints = constraints;
		return true;
	}
	return false;
}

bool FrameRequest::init(const Rc<Queue> &q, const Rc<FrameEmitter> &emitter, const gl::FrameContraints &constraints) {
	if (init(q)) {
		_emitter = emitter;
		_constraints = constraints;
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

void FrameRequest::addImageSpecialization(const ImageAttachment *image, gl::ImageInfoData &&data) {
	auto it = _imageSpecialization.find(image);
	if (it != _imageSpecialization.end()) {
		it->second = move(data);
	} else {
		_imageSpecialization.emplace(image, data);
	}
}

const gl::ImageInfoData *FrameRequest::getImageSpecialization(const ImageAttachment *image) const {
	auto it = _imageSpecialization.find(image);
	if (it != _imageSpecialization.end()) {
		return &it->second;
	}
	return nullptr;
}

bool FrameRequest::addInput(const Attachment *a, Rc<AttachmentInputData> &&data) {
	return addInput(a->getData(), move(data));
}

bool FrameRequest::addInput(const AttachmentData *a, Rc<AttachmentInputData> &&data) {
	if (a && a->attachment->validateInput(data)) {
		auto wIt = _waitForInputs.find(a);
		if (wIt != _waitForInputs.end()) {
			wIt->second.handle->submitInput(*wIt->second.queue, move(data), move(wIt->second.callback));
		} else {
			_input.emplace(a, move(data));
		}
		return true;
	}
	if (a) {
		log::vtext("FrameRequest", "Invalid input for attachment ", a->key);
	}
	return false;
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

void FrameRequest::setOutput(Rc<FrameOutputBinding> &&binding) {
	_output.emplace(binding->attachment, move(binding));
}

void FrameRequest::setOutput(const AttachmentData *a, CompleteCallback &&cb) {
	setOutput(Rc<FrameOutputBinding>::alloc(a, move(cb)));
}

void FrameRequest::setOutput(const AttachmentData *a, Rc<gl::View> &&view, CompleteCallback &&cb) {
	setOutput(Rc<FrameOutputBinding>::alloc(a, move(view), move(cb)));
}

void FrameRequest::setOutput(const Attachment *a, CompleteCallback &&cb) {
	setOutput(a->getData(), move(cb));
}

void FrameRequest::setOutput(const Attachment *a, Rc<gl::View> &&view, CompleteCallback &&cb) {
	setOutput(a->getData(), move(view), move(cb));
}

void FrameRequest::setRenderTarget(const AttachmentData *a, Rc<ImageStorage> &&img) {
	_renderTargets.emplace(a, move(img));
}

bool FrameRequest::onOutputReady(gl::Loop &loop, FrameAttachmentData &data) {
	auto it = _output.find(data.handle->getAttachment()->getData());
	if (it != _output.end()) {
		if (it->second->handleReady(data, true)) {
			_output.erase(it);
			return true;
		}
		return false;
	}

	return false;
}

void FrameRequest::onOutputInvalidated(gl::Loop &loop, FrameAttachmentData &data) {
	auto it = _output.find(data.handle->getAttachment()->getData());
	if (it != _output.end()) {
		if (it->second->handleReady(data, false)) {
			_output.erase(it);
			return;
		}
	}
}

void FrameRequest::finalize(gl::Loop &loop, HashMap<const AttachmentData *, FrameAttachmentData *> &attachments, bool success) {
	_waitForInputs.clear();

	if (!success) {
		for (auto &it : _output) {
			auto iit = attachments.find(it.second->attachment);
			if (iit != attachments.end()) {
				it.second->handleReady(*(iit->second), false);
			}
		}
		_output.clear();
	}
	if (_emitter) {
		_emitter = nullptr;
	}

	if (!_signalDependencies.empty()) {
		loop.signalDependencies(_signalDependencies, success);
	}
}

void FrameRequest::signalDependencies(gl::Loop &loop, bool success) {
	if (!_signalDependencies.empty()) {
		loop.signalDependencies(_signalDependencies, success);
		_signalDependencies.clear();
	}
}

Rc<AttachmentInputData> FrameRequest::getInputData(const AttachmentData *attachment) {
	auto it = _input.find(attachment);
	if (it != _input.end()) {
		auto ret = it->second;
		_input.erase(it);
		return ret;
	}
	return nullptr;
}

Rc<ImageStorage> FrameRequest::getRenderTarget(const AttachmentData *a) {
	auto it = _renderTargets.find(a);
	if (it != _renderTargets.end()) {
		return it->second;
	}
	return nullptr;
}

Set<Rc<Queue>> FrameRequest::getQueueList() const {
	return Set<Rc<Queue>>{_queue};
}

void FrameRequest::waitForInput(FrameQueue &queue, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	auto it = _waitForInputs.find(a->getAttachment()->getData());
	if (it != _waitForInputs.end()) {
		it->second.callback(false);
		it->second.callback = move(cb);
	} else {
		_waitForInputs.emplace(a->getAttachment()->getData(), WaitInputData{&queue, a, move(cb)});
	}
}

const FrameOutputBinding *FrameRequest::getOutputBinding(const AttachmentData *a) const {
	auto it = _output.find(a);
	if (it != _output.end()) {
		return it->second;
	}
	return nullptr;
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
uint64_t FrameEmitter::getAvgFenceTime() const {
	return _avgFenceIntervalValue;
}

bool FrameEmitter::isReadyForSubmit() const {
	return _frames.empty() && _framesPending.empty();
}

void FrameEmitter::setEnableBarrier(bool value) {
	_enableBarrier = value;
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

	if (auto t = frame.getSubmissionTime()) {
		_avgFenceInterval.addValue(t);
		_avgFenceIntervalValue = _avgFenceInterval.getAverage(true);
	}

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

	bool isAttachmentsDirty = req->isAttachmentsDirty();
	req->setReadyForSubmit(readyForSubmit);
	auto frame = _loop->makeFrame(move(req), _gen);

	enableCacheAttachments(frame, isAttachmentsDirty);

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
		[[maybe_unused]] auto t = platform::device::_clock(platform::device::ClockType::Monotonic);
		_loop->schedule([=, guard = Rc<FrameEmitter>(this), idx = _order] (gl::Loop &ctx) {
			XL_FRAME_EMITTER_LOG("TimeoutPassed:    ", _frame.load(), "   ", platform::device::_clock() - _frame.load(), " (",
					platform::device::_clock(platform::device::ClockType::Monotonic) - t, ") mks");
			guard->onFrameTimeout(idx);
			return true; // end spinning
		}, _frameInterval - config::FrameIntervalSafeOffset, "FrameEmitter::scheduleFrameTimeout");
	}
}

Rc<FrameRequest> FrameEmitter::makeRequest(const gl::FrameContraints &constraints) {
	_frame = platform::device::_clock();
	return Rc<FrameRequest>::create(this, constraints);
}

Rc<FrameHandle> FrameEmitter::submitNextFrame(Rc<FrameRequest> &&req) {
	if (!_valid) {
		return nullptr;
	}

	bool readyForSubmit = !_enableBarrier || (_frames.empty() && _framesPending.empty());
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

void FrameEmitter::enableCacheAttachments(const Rc<FrameHandle> &req, bool dirty) {
	auto &queues = req->getFrameQueues();

	Set<Rc<Queue>> list;
	for (auto &it : queues) {
		list.emplace(it->getRenderQueue());
	}

	if (_cacheRenderQueue != list || dirty) {
		Set<gl::ImageInfoData> images;
		for (auto &it : queues) {
			for (auto &a : it->getRenderQueue()->getAttachments()) {
				if (a->type == AttachmentType::Image) {
					auto img = (ImageAttachment *)a->attachment.get();
					gl::ImageInfoData data = img->getImageInfo();
					if (auto spec = req->getImageSpecialization(img)) {
						data = *spec;
					}
					data.extent = img->getSizeForFrame(*it);
					images.emplace(data);

					// for possible transient attachment add transient version of image
					if (a->transient) {
						data.usage |=  gl::ImageUsage::TransientAttachment;
						images.emplace(data);
					}
				}
			}
		}

		_cacheRenderQueue = list;

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
