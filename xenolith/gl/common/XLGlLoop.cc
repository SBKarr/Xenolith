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

#include "XLGlLoop.h"
#include "XLPlatform.h"
#include "XLApplication.h"
#include "XLDirector.h"

namespace stappler::xenolith::gl {

struct Loop::Internal : memory::AllocPool {
	Internal() {
		auto p = memory::pool::acquire();
		memory::pool::push(p);
		events = new (p) memory::vector<Event>(); events->reserve(4);
		eventsSwap = new (p) memory::vector<Event>(); eventsSwap->reserve(4);
		timers = new (p) memory::vector<Timer>(); timers->reserve(8);
		reschedule = new (p) memory::vector<Timer>(); reschedule->reserve(8);
		memory::pool::pop();
	}

	memory::vector<Event> *events;
	memory::vector<Event> *eventsSwap;

	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
};

struct PresentationData {
	PresentationData(uint64_t frameInterval)
	: frameInterval(frameInterval) { }

	uint64_t incrementTime() {
		auto tmp = now;
		now = platform::device::_clock();
		return now - tmp;
	}

	uint64_t getFrameInterval() {
		if (suboptimal > 0) {
			//return frameInterval * 2;
		}
		return frameInterval;
	}

	uint64_t getLastFrameInterval() {
		auto tmp = lastFrame;
		lastFrame = platform::device::_clock();
		return lastFrame - tmp;
	}

	uint64_t getLastUpdateInterval() {
		auto tmp = lastUpdate;
		lastUpdate = platform::device::_clock();
		return lastUpdate - tmp;
	}

	uint64_t now = platform::device::_clock();
	uint64_t last = 0;
	uint64_t frameInterval;
	uint64_t updateInterval = config::PresentationSchedulerInterval;
	uint64_t timer = 0;
	uint32_t suboptimal = 0; // counter for frames, allowed in suboptimal mode
	uint64_t frame = 0;
	uint64_t lastFrame = 0;
	uint64_t lastUpdate = 0;
	bool exit = false;
	bool swapchainValid = true;
};

Loop::Loop(Application *app, const Rc<View> &v, const Rc<Device> &dev, const Rc<Director> &dir, uint64_t frameMicroseconds)
: _application(app), _view(v), _device(dev), _director(dir), _frameTimeMicroseconds(frameMicroseconds) { }

Loop::~Loop() {

}

void Loop::threadInit() {
	thread::ThreadInfo::setThreadInfo("Gl::Loop");

	memory::pool::initialize();
	_pool = memory::pool::createTagged("Gl::Loop", mempool::custom::PoolFlags::ThreadSafeAllocator);

	_threadId = std::this_thread::get_id();

	memory::pool::push(_pool);

	_queue = Rc<thread::TaskQueue>::alloc(
			math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16)), nullptr, "Gl::Loop::Queue");
	_queue->spawnWorkers();
	_device->begin(_application, *_queue);
	_queue->waitForAll();

	_internal = new (_pool) Internal;

	_internal->events->emplace_back(Event::Update, nullptr, data::Value());

	memory::pool::pop();
}

bool Loop::worker() {
	PresentationData data(_frameTimeMicroseconds.load());

	auto pushFrame = [&] (FrameHandle *frame, ViewEvent::Value extra = 0) {
		// schedule task for director
		_application->performOnMainThread([dir = _application->getDirector(), frame] {
			dir->render(frame);
		});
		// force thread update
		_view->pushEvent(ViewEvent::Thread);
	};

	auto update = [&] {
		/*if (data.low > 0) {
			-- data.low;
			if (data.low == 0) {
				if (!_device->isBestPresentMode()) {
					pushTask(PresentationEvent::SwapChainForceRecreate);
				}
			}
		}
		data.frame = data.now;
		_frameQueue->update();
		_dataQueue->update();*/
	};

	auto tryPresentFrame = [&] (const Rc<FrameHandle> &frame, StringView tag, bool force = false) {
		/*if (force || data.frameInterval == 0 || data.now - data.frame > data.getFrameInterval() - data.updateInterval) {
#ifdef XL_LOOP_DEBUG
			auto clock = platform::device::_clock();
			auto syncIdx = frame->sync->idx;
#endif
			pushUpdate(frame, tag);
			auto ret = _device->present(frame);
			XL_LOOP_LOG(tag, ": ", platform::device::_clock() - clock, " ",
					data.getLastFrameInterval(), " ", data.now - data.frame, " ", data.getFrameInterval(), " ", data.low, " ",
					syncIdx, ":", frame->imageIdx, " - ", frame->order);
			if (!ret) {
				invalidateSwapchain(ViewEvent::SwapchainRecreation);
			} else {
				update();
			}
		} else {
			auto frameDelay = data.getFrameInterval() - (data.now - data.frame) - data.updateInterval;
			schedule([this, frame] (Vector<PresentationTask> &t) {
				t.emplace_back(PresentationEvent::FrameTimeoutPassed, frame.get(), data::Value());
				return true;
			}, frameDelay);
		}*/
	};

	auto pushUpdate = [&] (FrameHandle *frame, StringView reason) {
		if (!frame || frame->getOrder() == _device->getOrder()) {
			auto newFrame = _device->beginFrame(*this, *_device->getDefaultRenderQueue());
			if (newFrame && newFrame->isInputRequired()) {
				pushFrame(newFrame, ViewEvent::Update);
			} else if (newFrame) {
				_view->pushEvent(ViewEvent::Update);
			}
			XL_LOOP_LOG("Update: ", data.getLastUpdateInterval(), " ", data.getFrameInterval(), " ",
					data.low, " [", reason, "]  ", frame ? frame->order : 0);
		} else {
			XL_LOOP_LOG("Update: rejected [", reason, "]  ", frame ? frame->order : 0);
		}
	};

	auto invalidateSwapchain = [&] (ViewEvent::Value event) {
		if (data.swapchainValid) {
			data.timer += data.incrementTime();
			data.swapchainValid = false;

			_device->incrementGeneration();
			_queue->waitForAll();
			_view->pushEvent(event);
			data.suboptimal = 20;
			//pushUpdate(nullptr, "InvalidateSwapchain");
			//update();
		}
	};

	_running.store(true);

	auto pool = memory::pool::create(_pool);

	std::unique_lock<std::mutex> lock(_mutex);
	while (!data.exit) {
		bool timerPassed = false;
		do {
			Context context;
			context.events = _internal->events;
			_internal->events = _internal->eventsSwap;
			_internal->eventsSwap = context.events;

			do {
				if (!context.events->empty()) {
					data.timer += data.incrementTime();
					if (data.timer > data.updateInterval) {
						timerPassed = true;
					}
					break;
				} else {
					data.timer += data.incrementTime();
					if (data.timer < data.updateInterval) {
						if (!_cond.wait_for(lock, std::chrono::microseconds(data.updateInterval - data.timer), [&] {
							return !_internal->events->empty();
						})) {
							timerPassed = true;
						} else {
							context.events = _internal->events;
							_internal->events = _internal->eventsSwap;
							_internal->eventsSwap = context.events;
						}
					} else {
						timerPassed = true;
					}
				}
			} while (0);
			lock.unlock();

			_currentContext = &context;

			_queue->update();

			if (timerPassed) {
				auto now = platform::device::_clock();
				runTimers(now - data.last, context);
				data.last = now;
			}

			auto &events = *context.events;
			for (auto &it : events) {
				memory::pool::context<memory::pool_t *> ctx(pool);
				switch (it.event) {
				case Event::Update:
					pushUpdate(nullptr, "Update");
					break;
				case Event::SwapChainDeprecated:
					invalidateSwapchain(ViewEvent::SwapchainRecreation);
					break;
				case Event::SwapChainRecreated:
					data.swapchainValid = true; // resume drawing
					_device->reset(*_queue);
					pushUpdate(nullptr, "SwapChainRecreated");
					break;
				case Event::SwapChainForceRecreate:
					invalidateSwapchain(ViewEvent::SwapchainRecreationBest);
					break;
				case Event::FrameUpdate:
					if (auto frame = it.data.cast<FrameHandle>()) {
						frame->update();
					}
					break;
				/*case PresentationEvent::FrameImageAcquired:
					if (data.swapchainValid && _device->isFrameUsable((FrameData *)it.data.get())) {
						auto frame = it.data.cast<FrameData>();
						pushFrame(frame);
					}
					break;
				case PresentationEvent::FramePresentReady:
					// command buffers constructed - wait for next frame slot (or run immediately if framerate is low)
					if (data.swapchainValid && _device->isFrameUsable((FrameData *)it.data.get())) {
						auto frame = it.data.cast<FrameData>();
						frame->status = FrameStatus::PresentReady;
						if (frame->buffers.empty() || _device->getDraw()->submit(frame)) {
							tryPresentFrame(frame, "Present[R]");
						} else {
							invalidateSwapchain(ViewEvent::SwapchainRecreation);
						}
					}
					break;
				case PresentationEvent::FrameTimeoutPassed:
					if (data.swapchainValid && _device->isFrameUsable((FrameData *)it.data.get())) {
						auto frame = it.data.cast<FrameData>();
						tryPresentFrame(frame, "Present[T]", true);
					}
					break;*/
				case Event::UpdateFrameInterval:
					// view want us to change frame interval
					data.frameInterval = uint64_t(it.value.getInteger());
					break;
				case Event::CompileResource:
					_device->compileResource(*_queue, it.data.cast<Resource>());
					break;
				case Event::Exit:
					data.exit = true;
					break;
				}
			}

			_currentContext = nullptr;
			events.clear();
			memory::pool::clear(pool);
		} while (0);
		lock.lock();
	}

	memory::pool::clear(pool);

	_running.store(false);
	lock.unlock();

	memory::pool::push(_pool);
	_device->end(*_queue);
	memory::pool::pop();

	_queue->waitForAll();
	_queue->cancelWorkers();

	lock.lock();
	_internal->events->clear();
	_internal->eventsSwap->clear();
	_internal->timers->clear();
	_internal->reschedule->clear();
	_internal = nullptr;
	lock.unlock();

	memory::pool::destroy(_pool);
	memory::pool::terminate();

	return false;
}

void Loop::pushEvent(Event::EventName event, Rc<Ref> && data, data::Value &&value) {
	if (_running.load()) {
		std::unique_lock<std::mutex> lock(_mutex);
		_internal->events->emplace_back(event, move(data), move(value));
		_cond.notify_all();
	}
}

void Loop::pushContextEvent(Event::EventName event, Rc<Ref> && ref, data::Value && data) {
	if (std::this_thread::get_id() == _thread.get_id() && _currentContext) {
		for (auto &it : *_currentContext->events) {
			if (it.event == event && it.data == ref) {
				return;
			}
		}

		_currentContext->events->emplace_back(Event(event, move(ref), move(data)));
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb) {
	if (_running.load()) {
		std::unique_lock<std::mutex> lock(_mutex);
		_internal->timers->emplace_back(0, move(cb));
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb, uint64_t delay) {
	if (_running.load()) {
		std::unique_lock<std::mutex> lock(_mutex);
		_internal->timers->emplace_back(delay, move(cb));
	}
}

void Loop::begin() {
	_thread = StdThread(thread::ThreadHandlerInterface::workerThread, this, nullptr);
}

void Loop::end(bool success) {
	pushEvent(Event::Exit, nullptr, data::Value(success));
	_thread.join();
}

void Loop::performOnThread(const Function<void()> &func, Ref *target) {
	_queue->onMainThread(Rc<thread::Task>::create([func] (const thread::Task &, bool success) {
		if (success) { func(); }
	}, target));
	_cond.notify_all();
}

void Loop::wake() {
	_cond.notify_all();
}

void Loop::runTimers(uint64_t dt, Context &t) {
	_mutex.lock();
	auto timers = _internal->timers;
	_internal->timers = _internal->reschedule;
	_mutex.unlock();

	auto it = timers->begin();
	while (it != timers->end()) {
		if (it->interval) {
			it->value += dt;
			if (it->value > it->interval) {
				auto ret = it->callback(t);
				if (!ret) {
					it->value -= it->interval;
				} else {
					it = timers->erase(it);
					continue;
				}
			}
			++ it;
		} else {
			auto ret = it->callback(t);
			if (ret) {
				it = timers->erase(it);
			} else {
				++ it;
			}
		}
	}

	_mutex.lock();
	if (!_internal->timers->empty()) {
		for (auto &it : *_internal->timers) {
			timers->emplace_back(std::move(it));
		}
		_internal->timers->clear();
	}
	_internal->timers = timers;
	_mutex.unlock();
}

}
