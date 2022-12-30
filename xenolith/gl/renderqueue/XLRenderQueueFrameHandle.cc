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

#include "XLRenderQueueFrameHandle.h"
#include "XLRenderQueueFrameQueue.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::renderqueue {

static constexpr platform::device::ClockType FrameClockType = platform::device::ClockType::Monotonic;

#ifdef XL_FRAME_LOG
#define XL_FRAME_LOG_INFO _request->getEmitter() ? "[Emitted] " : "", \
	"[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), \
	"] [", platform::device::_clock(FrameClockType) - _timeStart, "] "
#endif

static std::atomic<uint32_t> s_frameCount = 0;

static Mutex s_frameMutex;
static std::set<FrameHandle *> s_activeFrames;

uint32_t FrameHandle::GetActiveFramesCount() {
	return s_frameCount.load();
}

void FrameHandle::DescribeActiveFrames() {
	s_frameMutex.lock();
#if SP_REF_DEBUG
	auto hasFailed = false;
	for (auto &it : s_activeFrames) {
		if (!it->isValidFlag()) {
			hasFailed = true;
			break;
		}
	}

	if (hasFailed) {
		StringStream stream;
		stream << "\n";
		for (auto &it : s_activeFrames) {
			stream << "\tFrame: " << it->getOrder() << " refcount: " << it->getReferenceCount() << "; success: " << it->isValidFlag() << "; backtrace:\n";
			it->foreachBacktrace([&] (uint64_t id, Time time, const std::vector<std::string> &vec) {
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) {
					stream << "\t" << it << "\n";
				}
			});
		}
		stappler::log::text("FrameHandle", stream.str());
	}
#endif

	s_frameMutex.unlock();
}

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Destroy");

	s_frameMutex.lock();
	-- s_frameCount;
	s_activeFrames.erase(this);
	s_frameMutex.unlock();

	_request = nullptr;
	_pool = nullptr;
}

bool FrameHandle::init(gl::Loop &loop, gl::Device &dev, Rc<FrameRequest> &&req, uint64_t gen) {
	s_frameMutex.lock();
	s_activeFrames.emplace(this);
	++ s_frameCount;
	s_frameMutex.unlock();

	_loop = &loop;
	_device = &dev;
	_request = move(req);
	_pool = _request->getPool();
	_timeStart = platform::device::_clock(FrameClockType);
	if (!_request || !_request->getQueue()) {
		return false;
	}

	_gen = gen;
	_order = _request->getQueue()->incrementOrder();
	if (const auto &target = _request->getRenderTarget()) {
		target->setFrameIndex(_order);
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "Init; ready: ", _request->isReadyForSubmit());
	return setup();
}

void FrameHandle::update(bool init) {
	if (!_valid) {
		return;
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "update");

	for (auto &it : _queues) {
		it->update();
	}
}

gl::ImageInfoData FrameHandle::getImageSpecialization(const ImageAttachment *a) const {
	gl::ImageInfoData ret;
	if (auto img = _request->getImageSpecialization(a)) {
		ret = *img;
	} else {
		ret = a->getImageInfo();
	}
	return ret;
}

bool FrameHandle::isSwapchainAttachment(const Attachment *a) const {
	return _request->isSwapchainAttachment(a);
}

void FrameHandle::schedule(Function<bool(FrameHandle &)> &&cb, StringView tag) {
	auto linkId = retain();
	_loop->schedule([this, cb = move(cb), linkId] (gl::Loop &ctx) {
		if (!isValid()) {
			release(linkId);
			return true;
		}
		if (cb(*this)) {
			release(linkId);
			return true; // end
		}
		return false;
	}, tag);
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [=, this] (const thread::Task &, bool) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [=, this, complete = move(complete)] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate, StringView tag) {
	if (immediate && _loop->isOnGlThread()) {
		XL_FRAME_PROFILE(cb(*this), tag, 1000);
	} else {
		auto linkId = retain();
		_loop->performOnGlThread(Rc<thread::Task>::create([=, this, cb = move(cb)] (const thread::Task &, bool success) {
			XL_FRAME_PROFILE(if (success) { cb(*this); }, tag, 1000);
			XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
			release(linkId);
		}, ref));
	}
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		return cb(*this);
	}, [this, linkId, tag] (const thread::Task &, bool success) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (success) {
			onRequiredTaskCompleted(tag);
		} else {
			invalidate();
		}
		release(linkId);
	}, ref));
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->performInQueue(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete), linkId, tag] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "thread performed: '", tag, "'");
		if (success) {
			onRequiredTaskCompleted(tag);
		} else {
			invalidate();
		}
		release(linkId);
	}, ref));
}

bool FrameHandle::isValid() const {
	return _valid && (!_request->getEmitter() || _request->getEmitter()->isFrameValid(*this));
}

bool FrameHandle::isPersistentMapping() const {
	return _request->isPersistentMapping();
}

Rc<AttachmentInputData> FrameHandle::getInputData(const Attachment *attachment) {
	return _request->getInputData(attachment);
}

void FrameHandle::setReadyForSubmit(bool value) {
	if (!isValid()) {
		XL_FRAME_LOG(XL_FRAME_LOG_INFO, "[invalid] frame ready to submit");
		return;
	}

	XL_FRAME_LOG(XL_FRAME_LOG_INFO, "frame ready to submit");
	_request->setReadyForSubmit(value);
	if (_request->isReadyForSubmit()) {
		_loop->performOnGlThread([this] {
			update();
		}, this);
	}
}

void FrameHandle::invalidate() {
	if (_loop->isOnGlThread()) {
		if (_valid) {
			if (!_timeEnd) {
				_timeEnd = platform::device::_clock(FrameClockType);
			}

			if (auto e = _request->getEmitter()) {
				XL_FRAME_LOG(XL_FRAME_LOG_INFO, "complete: ", e->getFrameTime());
			}

			_valid = false;
			_completed = true;
			for (auto &it : _queues) {
				it->invalidate();
			}

			if (!_submitted) {
				_submitted = true;
				if (_request->getEmitter()) {
					_request->getEmitter()->setFrameSubmitted(*this);
				}
			}

			if (_complete) {
				_complete(*this);
			}

			if (_request) {
				_request->finalize(*_loop, _valid);
			}
		}
	} else {
		_loop->performOnGlThread([this] {
			invalidate();
		}, this);
	}
}

void FrameHandle::setCompleteCallback(Function<void(FrameHandle &)> &&cb) {
	_complete = move(cb);
}

bool FrameHandle::setup() {

	_pool->perform([&] {
		auto q = Rc<FrameQueue>::create(_pool, _request->getQueue(), *this, _request->getExtent());
		q->setup();

		_queues.emplace_back(move(q));
	});

	if (!_valid) {
		for (auto &it : _queues) {
			it->invalidate();
		}
	}

	return true;
}

void FrameHandle::onQueueSubmitted(FrameQueue &queue) {
	++ _queuesSubmitted;
	if (_queuesSubmitted == _queues.size()) {
		_submitted = true;
		if (_request->getEmitter()) {
			_request->getEmitter()->setFrameSubmitted(*this);
		}
	}
}

void FrameHandle::onQueueComplete(FrameQueue &queue) {
	_submissionTime += queue.getSubmissionTime();
	++ _queuesCompleted;
	tryComplete();
}

void FrameHandle::onRequiredTaskCompleted(StringView tag) {
	++ _tasksCompleted;
	tryComplete();
}

void FrameHandle::onOutputAttachment(FrameAttachmentData &data) {
	if (_request->onOutputReady(*_loop, data)) {
		data.image = nullptr;
		data.state = FrameAttachmentState::Detached;
	}
}

void FrameHandle::onOutputAttachmentInvalidated(FrameAttachmentData &data) {
	_request->onOutputInvalidated(*_loop, data);
}

void FrameHandle::waitForDependencies(const Vector<Rc<DependencyEvent>> &events, Function<void(FrameHandle &, bool)> &&cb) {
	auto linkId = retain();
	_loop->waitForDependencies(events, [this, cb = move(cb), linkId] (bool success) {
		cb(*this, success);
		release(linkId);
	});
}

void FrameHandle::waitForInput(FrameQueue &queue, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb) {
	_request->waitForInput(queue, a, move(cb));
}

void FrameHandle::signalDependencies(bool success) {
	_request->signalDependencies(*_loop, success);
}

void FrameHandle::onQueueInvalidated(FrameQueue &) {
	++ _queuesCompleted;
	invalidate();
}

void FrameHandle::tryComplete() {
	if (_tasksCompleted == _tasksRequired.load() && _queuesCompleted == _queues.size()) {
		onComplete();
	}
}

void FrameHandle::onComplete() {
	if (!_completed && _valid) {
		_timeEnd = platform::device::_clock(FrameClockType);
		if (auto e = getEmitter()) {
			XL_FRAME_LOG(XL_FRAME_LOG_INFO, "complete: ", e->getFrameTime());
		}
		_completed = true;

		if (_complete) {
			_complete(*this);
		}

		_request->finalize(*_loop, _valid);
	}
}

}
