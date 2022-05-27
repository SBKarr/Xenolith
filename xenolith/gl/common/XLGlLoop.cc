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
		timers = new (p) memory::vector<Timer>(); timers->reserve(8);
		reschedule = new (p) memory::vector<Timer>(); reschedule->reserve(8);
		autorelease = new (p) memory::vector<Rc<Ref>>(); autorelease->reserve(8);
		memory::pool::pop();
	}

	memory::vector<Event> *events;
	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
	memory::vector<Rc<Ref>> *autorelease;
};

struct PresentationData {
	PresentationData() { }

	uint64_t getLastUpdateInterval() {
		auto tmp = lastUpdate;
		lastUpdate = platform::device::_clock(platform::device::ClockType::Monotonic);
		return lastUpdate - tmp;
	}

	uint64_t now = platform::device::_clock(platform::device::ClockType::Monotonic);
	uint64_t last = 0;
	uint64_t updateInterval = config::PresentationSchedulerInterval;
	uint64_t lastUpdate = 0;
	bool exit = false;

	uint32_t events = 0;
	uint32_t timers = 0;
	uint32_t tasks = 0;
};

Loop::Loop(Application *app, const Rc<Device> &dev)
: _application(app), _device(dev) {
	_queue = Rc<TaskQueue>::alloc("Gl::Loop::Queue");
	_queue->spawnWorkers(TaskQueue::Flags::Waitable | TaskQueue::Flags::Cancelable, LoopThreadId,
			math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16)));
	//_queue->spawnWorkers(LoopThreadId, math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16)));
}

Loop::~Loop() {
	if (_device) {
		_device = nullptr;
	}
}

void Loop::threadInit() {
	thread::ThreadInfo::setThreadInfo("Gl::Loop");

	memory::pool::initialize();
	_pool = memory::pool::createTagged("Gl::Loop", mempool::custom::PoolFlags::ThreadSafeAllocator);

	_threadId = std::this_thread::get_id();

	memory::pool::push(_pool);

	_internal = new (_pool) Internal;

	memory::pool::pop();
}

bool Loop::worker() {
	PresentationData data;

	/*auto invalidateSwapchain = [&] (Swapchain *swapchain, AppEvent::Value event) {
		if (swapchain->isValid()) {
			swapchain->incrementGeneration(event);
		}
	};*/

	_running.store(true);

	_pendingEventMutex.lock();
	for (auto &it : _pendingEvents) {
		_internal->events->emplace_back(move(it));
	}

	_pendingEvents.clear();
	_pendingEventMutex.unlock();

	_device->onLoopStarted(*this);

	auto pool = memory::pool::create(_pool);

	while (!data.exit) {
		bool timerPassed = false;
		do {
			++ _clock;

			XL_PROFILE_BEGIN(loop, "gl::Loop", "loop", 1000);

			data.events = 0;
			data.timers = 0;
			data.tasks = 0;

			Context context;
			context.events = _internal->events;
			context.loop = this;

			_currentContext = &context;

			XL_PROFILE_BEGIN(poll, "gl::Loop::Poll", "poll", 500);
			timerPassed = pollEvents(data);
			XL_PROFILE_END(poll)

			if (timerPassed) {
				auto dt = data.now - data.last;
				XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", "timers", 500);
				data.timers += runTimers(dt, context);
				XL_PROFILE_END(timers)
				data.last = data.now;
			}

			memory::vector<Event> *tmpEvents = nullptr;
			do {
				memory::pool::context<memory::pool_t *> ctx(pool);
				tmpEvents = new (pool) memory::vector<Event>;
			} while (0);

			auto nextEvents = tmpEvents;
			auto origEvents = context.events;
			while (!context.events->empty()) {
				memory::pool::context<memory::pool_t *> ctx(pool);

				// swap array to correctly handle extra context events
				auto events = context.events;
				context.events = nextEvents;
				nextEvents = events;

				auto it = events->begin();
				while (it != events->end()) {
					++ data.events;

					XL_PROFILE_BEGIN(events, "gl::Loop::Event", getEventName(it->event), 500);

					switch (it->event) {
					case EventName::Update:
						if (auto s = (FrameEmitter *)it->data.get()) {
							s->acquireNextFrame();
						} else {
							log::text("gl::Loop", "Event::Update without FrameEmitter");
						}
						break;
					case EventName::FrameUpdate:
						if (auto frame = it->data.cast<FrameHandle>()) {
							frame->update();
						} else {
							log::text("gl::Loop", "Event::FrameUpdate without frame");
						}
						break;
					case EventName::FrameInvalidated:
						if (auto frame = it->data.cast<FrameHandle>()) {
							frame->invalidate();
						} else {
							log::text("gl::Loop", "Event::FrameInvalidated without frame");
						}
						break;
					case EventName::CompileResource:
						_device->compileResource(*this, it->data.cast<Resource>(), move(it->callback));
						break;
					case EventName::CompileMaterials:
						_device->compileMaterials(*this, it->data.cast<MaterialInputData>());
						break;
					case EventName::RunRenderQueue:
						if (auto data = (FrameRequest *)it->data.get()) {
							auto frame = _device->makeFrame(*this, data, it->value.getInteger());
							if (it->callback) {
								frame->setCompleteCallback([cb = move(it->callback)] (FrameHandle &handle) {
									cb(handle.isValid());
								});
							}
							frame->update(true);
						}
						break;
					case EventName::Exit:
						data.exit = true;
						break;
					}

					XL_PROFILE_END(events)

					++ it;
				}

				events->clear();
			}

			context.events = origEvents;
			_currentContext = nullptr;
			context.events->clear();
			XL_PROFILE_BEGIN(autorelease, "gl::Loop::Autorelease", "autorelease", 500);
			_internal->autorelease->clear();
			XL_PROFILE_END(autorelease)

			XL_PROFILE_END(loop)
			memory::pool::clear(pool);
		} while (0);
		// log::vtext("gl::Loop", "Clock: ", platform::device::_clock() - data.now, "(", data.events, ", ", data.timers, ", ", data.tasks, ")");
	}

	memory::pool::clear(pool);

	_queue->lock();

	_device->onLoopEnded(*this);
	_device->waitIdle(*this);

	_running.store(false);
	_queue->unlock();

	_queue->waitForAll();

	_queue->lock();
	memory::pool::push(_pool);
	_device->end(*this, *_queue);
	memory::pool::pop();
	_queue->unlock();

	_queue->waitForAll();

	_queue->lock();
	_internal->events->clear();
	_internal->timers->clear();
	_internal->reschedule->clear();
	_internal->autorelease->clear();
	_internal = nullptr;
	_queue->unlock();

	_pendingEventMutex.lock();
	_pendingEvents.clear();
	_pendingEventMutex.unlock();

	_queue->cancelWorkers();

	memory::pool::destroy(_pool);
	memory::pool::terminate();

	return false;
}

void Loop::pushEvent(EventName event, Rc<Ref> && data, Value &&value, Function<void(bool)> &&cb) {
	if (_running.load()) {
		performOnThread([this, event, data = move(data), value = move(value), cb = move(cb)] () mutable {
			_internal->events->emplace_back(event, move(data), move(value), move(cb));
		});
	} else {
		_pendingEventMutex.lock();
		_pendingEvents.emplace_back(event, move(data), move(value), move(cb));
		_pendingEventMutex.unlock();
	}
}

void Loop::pushContextEvent(EventName event, Rc<Ref> && ref, Value && data, Function<void(bool)> &&cb) {
	if (std::this_thread::get_id() == _thread.get_id() && _currentContext) {
		for (auto &it : *_currentContext->events) {
			if (it.event == event && it.data == ref) {
				return;
			}
		}

		_currentContext->events->emplace_back(Event(event, move(ref), move(data), move(cb)));
	} else {
		pushEvent(event, move(ref), move(data), move(cb));
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb, StringView tag) {
	XL_ASSERT(isOnThread(), "Gl-Loop: schedule should be called in GL thread");
	if (_running.load()) {
		if (isOnThread()) {
			_internal->timers->emplace_back(0, move(cb), tag);
		} else {
			performOnThread([this, cb = move(cb), tag] () mutable {
				_internal->timers->emplace_back(0, move(cb), tag);
			});
		}
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb, uint64_t delay, StringView tag) {
	XL_ASSERT(isOnThread(), "Gl-Loop: schedule should be called in GL thread");
	if (_running.load()) {
		if (isOnThread()) {
			_internal->timers->emplace_back(delay, move(cb), tag);
		} else {
			performOnThread([this, cb = move(cb), delay, tag] () mutable {
				_internal->timers->emplace_back(delay, move(cb), tag);
			});
		}
	}
}

void Loop::begin() {
	_device->begin(_application, *_queue);

	// then start loop itself
	_thread = std::thread(Loop::workerThread, this, nullptr);
}

void Loop::compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> &&complete) {
	_device->compileRenderQueue(*this, req, move(complete));
}

void Loop::compileImage(const Rc<DynamicImage> &image, Function<void(bool)> &&complete) {
	_device->compileImage(*this, image, move(complete));
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&complete) {
	pushEvent(EventName::RunRenderQueue, move(req), Value(gen), move(complete));
}

void Loop::end(bool success) {
	pushEvent(EventName::Exit, nullptr, Value(success));
	_thread.join();
}

const Instance *Loop::getInstance() const {
	return _application->getGlInstance();
}

void Loop::performOnThread(Function<void()> &&func, Ref *target, bool immediate) {
	if (immediate) {
		if (isOnThread()) {
			func();
			return;
		}
	}

	_queue->onMainThread(move(func), target);
}

bool Loop::isOnThread() const {
	return std::this_thread::get_id() == _thread.get_id();
}

void Loop::autorelease(Ref *ref) {
	if (std::this_thread::get_id() == _thread.get_id()) {
		_internal->autorelease->emplace_back(ref);
	}
}

bool Loop::pollEvents(PresentationData &data) {
	bool timerPassed = false;
	auto counter = _queue->getOutputCounter();
	if (counter > 0) {
		XL_PROFILE_BEGIN(queue, "gl::Loop::Queue", "queue", 500);
		_queue->update(&data.tasks);
		XL_PROFILE_END(queue)

		data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
		if (data.now - data.last > data.updateInterval) {
			timerPassed = true;
		}
	} else {
		data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
		if (data.now - data.last > data.updateInterval) {
			timerPassed = true;
		} else {
			if (_internal->timers->empty()) {
				// no timers - just wait for events with 60FPS wakeups
				auto t = std::max(data.updateInterval, uint64_t(1'000'000 / 60));
				_queue->wait(TimeInterval::microseconds(t));
			} else {
				if (!_queue->wait(TimeInterval::microseconds(data.updateInterval - (data.now - data.last)))) {
					data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
					timerPassed = true;
				}
			}
		}
	}
	return timerPassed;
}

uint32_t Loop::runTimers(uint64_t dt, Context &t) {
	uint32_t ret = 0;
	auto timers = _internal->timers;
	_internal->timers = _internal->reschedule;

	auto it = timers->begin();
	while (it != timers->end()) {
		++ ret;
		if (it->interval) {
			it->value += dt;
			if (it->value > it->interval) {

				XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", it->tag, 1000);

				auto ret = it->callback(t);

				XL_PROFILE_END(timers);

				if (!ret) {
					it->value -= it->interval;
				} else {
					it = timers->erase(it);
					continue;
				}
			}
			++ it;
		} else {
			XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", it->tag, 1000);

			auto ret = it->callback(t);

			XL_PROFILE_END(timers);

			if (ret) {
				it = timers->erase(it);
			} else {
				++ it;
			}
		}
	}

	if (!_internal->timers->empty()) {
		for (auto &it : *_internal->timers) {
			timers->emplace_back(std::move(it));
		}
		_internal->timers->clear();
	}
	_internal->timers = timers;
	return ret;
}

StringView Loop::getEventName(EventName event) {
	switch (event) {
	case EventName::Update: return StringView("EventName::Update"); break;
	case EventName::FrameUpdate: return StringView("EventName::FrameUpdate"); break;
	case EventName::FrameInvalidated: return StringView("EventName::FrameInvalidated"); break;
	case EventName::CompileResource: return StringView("EventName::CompileResource"); break;
	case EventName::CompileMaterials: return StringView("EventName::CompileMaterials"); break;
	case EventName::RunRenderQueue: return StringView("EventName::RunRenderQueue"); break;
	case EventName::Exit: return StringView("EventName::Exit"); break;
	}
	return StringView();
}

}
