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

#include "XLGlFrameHandle.h"
#include "XLGlLoop.h"
#include "XLGlFrameQueue.h"

namespace stappler::xenolith::gl {

static std::atomic<uint32_t> s_frameCount = 0;

uint32_t FrameHandle::GetActiveFramesCount() {
	return s_frameCount.load();
}

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Destroy");

	-- s_frameCount;

	_request->finalize();
}

bool FrameHandle::init(Loop &loop, Rc<FrameRequest> &&req, uint64_t gen) {
	++ s_frameCount;
	_loop = &loop;
	_request = move(req);
	_timeStart = platform::device::_clock(platform::device::Process);
	if (!_request || !_request->getQueue()) {
		return false;
	}

	_gen = gen;
	_order = _request->getQueue()->incrementOrder();
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Init (", loop.getDevice()->getFramesActive(), ")");
	_device = loop.getDevice();
	_request->acquireInput(_inputData);
	return setup();
}

void FrameHandle::update(bool init) {
	if (!_valid) {
		return;
	}

	for (auto &it : _queues) {
		it->update();
	}
}

void FrameHandle::schedule(Function<bool(FrameHandle &, Loop::Context &)> &&cb, StringView tag) {
	auto linkId = retain();
	_loop->schedule([this, cb = move(cb), linkId] (Loop::Context &ctx) {
		if (!isValid()) {
			release(linkId);
			return true;
		}
		if (cb(*this, ctx)) {
			release(linkId);
			return true; // end
		}
		return false;
	}, tag);
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [this, linkId, tag] (const thread::Task &, bool) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete), linkId, tag] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate, StringView tag) {
	if (immediate && _loop->isOnThread()) {
		XL_FRAME_PROFILE(cb(*this), tag, 1000);
	} else {
		auto linkId = retain();
		_loop->getQueue()->onMainThread(Rc<thread::Task>::create([this, cb = move(cb), linkId, tag] (const thread::Task &, bool success) {
			XL_FRAME_PROFILE(if (success) { cb(*this); }, tag, 1000);
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
			release(linkId);
		}, ref));
	}
}

void FrameHandle::performRequiredTask(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [this, linkId, tag] (const thread::Task &, bool) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		onRequiredTaskCompleted(tag);
		release(linkId);
	}, ref));
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete), linkId, tag] (const thread::Task &, bool success) {
		XL_FRAME_PROFILE(complete(*this, success), tag, 1000);
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		onRequiredTaskCompleted(tag);
		release(linkId);
	}, ref));
}

bool FrameHandle::isValid() const {
	return _valid && (!_request->getEmitter() || _request->getEmitter()->isFrameValid(*this));
}

Rc<AttachmentInputData> FrameHandle::getInputData(const Attachment *attachment) {
	auto it = _inputData.find(attachment);
	if (it != _inputData.end()) {
		auto ret = it->second;
		_inputData.erase(it);
		return ret;
	}
	return nullptr;
}

void FrameHandle::setReadyForSubmit(bool value) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] frame ready to submit");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame ready to submit");
	_request->setReadyForSubmit(value);
	if (_request->isReadyForSubmit()) {
		_loop->pushContextEvent(Loop::EventName::FrameUpdate, this);
	}
}

void FrameHandle::invalidate() {
	if (_loop->isOnThread()) {
		if (_valid) {
			if (!_timeEnd) {
				_timeEnd = platform::device::_clock(platform::device::Process);
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
		}
	} else {
		_loop->performOnThread([this] {
			invalidate();
		}, this);
	}
}

void FrameHandle::setCompleteCallback(Function<void(FrameHandle &)> &&cb) {
	_complete = move(cb);
}

bool FrameHandle::setup() {
	_pool = Rc<PoolRef>::alloc(nullptr);

	_pool->perform([&] {
		auto q = Rc<FrameQueue>::create(_pool, _request->getQueue(), _request->getCache(), *this, _request->getExtent());
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
	++ _queuesCompleted;
	tryComplete();
}

void FrameHandle::onRequiredTaskCompleted(StringView tag) {
	++ _tasksCompleted;
	tryComplete();
}

void FrameHandle::onOutputAttachment(FrameQueueAttachmentData &data) {
	if (_request->onOutputReady(*_loop, data)) {
		data.image = nullptr;
		data.state = FrameAttachmentState::Detached;
	}
}

void FrameHandle::onQueueInvalidated(FrameQueue &) {
	++ _queuesCompleted;

	if (_valid && !_completed) {
		_loop->pushContextEvent(Loop::EventName::FrameInvalidated, this);
	}
}

void FrameHandle::tryComplete() {
	if (_tasksCompleted == _tasksRequired.load() && _queuesCompleted == _queues.size()) {
		onComplete();
	}
}

void FrameHandle::onComplete() {
	if (!_completed && _valid) {
		_timeEnd = platform::device::_clock(platform::device::Process);
		if (auto e = getEmitter()) {
			XL_FRAME_LOG("FrameTime:         ", e->getFrameTime(), "   ", _timeEnd - _timeStart, " mks");
		}
		_completed = true;

		if (_complete) {
			_complete(*this);
		}
	}
}

}
