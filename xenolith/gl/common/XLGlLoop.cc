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
		autorelease = new (p) memory::vector<Rc<Ref>>(); autorelease->reserve(8);
		memory::pool::pop();
	}

	memory::vector<Event> *events;
	memory::vector<Event> *eventsSwap;

	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
	memory::vector<Rc<Ref>> *autorelease;
};

struct PresentationData {
	PresentationData() { }

	uint64_t getLastUpdateInterval() {
		auto tmp = lastUpdate;
		lastUpdate = platform::device::_clock();
		return lastUpdate - tmp;
	}

	uint64_t now = platform::device::_clock();
	uint64_t last = 0;
	uint64_t updateInterval = config::PresentationSchedulerInterval;
	uint64_t lastUpdate = 0;
	bool exit = false;
};

Loop::Loop(Application *app, const Rc<Device> &dev)
: _application(app), _device(dev) {
	_queue = Rc<thread::TaskQueue>::alloc(
			math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16)),
			nullptr, "Gl::Loop::Queue", [this] {
		_cond.notify_all();
	});
	_queue->spawnWorkers();
}

Loop::~Loop() {

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

	auto invalidateSwapchain = [&] (Swapchain *swapchain, AppEvent::Value event) {
		if (swapchain->isValid()) {
			swapchain->incrementGeneration(event);
		}
	};

	_running.store(true);

	_mutex.lock();
	for (auto &it : _pendingEvents) {
		_internal->events->emplace_back(move(it));
	}

	_pendingEvents.clear();
	_mutex.unlock();

	_device->onLoopStarted(*this);

	auto pool = memory::pool::create(_pool);

	std::unique_lock<std::mutex> lock(_mutex);
	while (!data.exit) {
		bool timerPassed = false;
		do {
			++ _clock;
			Context context;
			context.events = _internal->events;
			_internal->events = _internal->eventsSwap;
			_internal->eventsSwap = context.events;

			timerPassed = pollEvents(lock, data, context);

			_currentContext = &context;

			_queue->update();

			uint64_t now = 0;
			if (timerPassed) {
				now = platform::device::_clock();
				auto dt = now - data.last;
				runTimers(dt, context);
				data.last = now;
				// log::vtext("Dt", data.updateInterval, " - ", dt);
			}

			auto &events = *context.events;
			auto it = events.begin();
			while (it != events.end()) {
				memory::pool::context<memory::pool_t *> ctx(pool);
				switch (it->event) {
				case EventName::Update:
					if (auto s = (Swapchain *)it->data.get()) {
						s->beginFrame(*this);
					} else {
						log::text("gl::Loop", "Event::Update without swapchain");
					}
					break;
				case EventName::SwapChainDeprecated:
					if (auto s = (Swapchain *)it->data.get()) {
						invalidateSwapchain(s, AppEvent::SwapchainRecreation);
					} else {
						log::text("gl::Loop", "Event::SwapChainDeprecated without swapchain");
					}
					break;
				case EventName::SwapChainRecreated:
					if (auto s = (Swapchain *)it->data.get()) {
						s->beginFrame(*this, true);
					} else {
						log::text("gl::Loop", "Event::SwapChainRecreated without swapchain");
					}
					break;
				case EventName::SwapChainForceRecreate:
					if (auto s = (Swapchain *)it->data.get()) {
						invalidateSwapchain(s, AppEvent::SwapchainRecreationBest);
					} else {
						log::text("gl::Loop", "Event::SwapChainForceRecreate without swapchain");
					}
					break;
				case EventName::FrameUpdate:
					if (auto frame = it->data.cast<FrameHandle>()) {
						frame->update();
					} else {
						log::text("gl::Loop", "Event::FrameUpdate without frame");
					}
					break;
				case EventName::FrameSubmitted:
					if (auto s = (Swapchain *)it->data.get()) {
						if (s->isResetRequired()) {
							pushEvent(EventName::SwapChainForceRecreate, s);
						} else if (s->isValid()) {
							auto frameInterval = s->getFrameInterval();
							if (frameInterval == 0) {
								s->beginFrame(*this);
							} else {
								if (now == 0) {
									now = platform::device::_clock();
								}
								auto timeFromFrame = (now - s->getFrameTime());
								if (timeFromFrame >= frameInterval) {
									s->beginFrame(*this);
								} else {
									schedule([this, s = it->data] (Context &context) {
										context.events->emplace_back(EventName::FrameTimeoutPassed, s.get(), data::Value());
										return true;
									}, frameInterval - timeFromFrame);
								}
							}
						}
					} else {
						log::text("gl::Loop", "Event::FrameSubmitted without swapchain");
					}
					break;
				case EventName::FrameTimeoutPassed:
					if (auto s = (Swapchain *)it->data.get()) {
						s->beginFrame(*this);
					} else {
						log::text("gl::Loop", "Event::FrameTimeoutPassed without swapchain");
					}
					break;
				case EventName::UpdateFrameInterval:
					if (auto s = (Swapchain *)it->data.get()) {
					// view want us to change frame interval
						s->setFrameInterval(it->value.getInteger());
					} else {
						log::text("gl::Loop", "Event::UpdateFrameInterval without swapchain");
					}
					break;
				case EventName::CompileResource:
					_device->compileResource(*_queue, it->data.cast<Resource>());
					break;
				case EventName::CompileMaterials:
					_device->compileMaterials(*this, it->data.cast<MaterialInputData>());
					break;
				case EventName::Exit:
					data.exit = true;
					break;
				}
				++ it;
			}

			_currentContext = nullptr;
			events.clear();
			_internal->autorelease->clear();
			memory::pool::clear(pool);
		} while (0);
		lock.lock();
	}

	memory::pool::clear(pool);

	_device->onLoopEnded(*this);
	_device->waitIdle();

	_running.store(false);
	lock.unlock();

	_queue->waitForAll();

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
	_internal->autorelease->clear();
	_internal = nullptr;
	lock.unlock();

	memory::pool::destroy(_pool);
	memory::pool::terminate();

	return false;
}

void Loop::pushEvent(EventName event, Rc<Ref> && data, data::Value &&value, Function<void(bool)> &&cb) {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_running.load()) {
		_internal->events->emplace_back(event, move(data), move(value), move(cb));
		_cond.notify_all();
	} else {
		_pendingEvents.emplace_back(event, move(data), move(value), move(cb));
	}
}

void Loop::pushContextEvent(EventName event, Rc<Ref> && ref, data::Value && data, Function<void(bool)> &&cb) {
	if (std::this_thread::get_id() == _thread.get_id() && _currentContext) {
		for (auto &it : *_currentContext->events) {
			if (it.event == event && it.data == ref) {
				return;
			}
		}

		_currentContext->events->emplace_back(Event(event, move(ref), move(data), move(cb)));
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb) {
	XL_ASSERT(isOnThread(), "Gl-Loop: schedule should be called in GL thread");
	if (_running.load()) {
		_internal->timers->emplace_back(0, move(cb));
	}
}

void Loop::schedule(Function<bool(Context &)> &&cb, uint64_t delay) {
	XL_ASSERT(isOnThread(), "Gl-Loop: schedule should be called in GL thread");
	if (_running.load()) {
		_internal->timers->emplace_back(delay, move(cb));
	}
}

void Loop::begin() {
	_device->begin(_application, *_queue);

	// then start loop itself
	_thread = StdThread(thread::ThreadHandlerInterface::workerThread, this, nullptr);
}

void Loop::compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> &&complete) {
	_device->compileRenderQueue(*this, req, move(complete));
}

void Loop::scheduleSwapchainRenderQueue(const Rc<RenderQueue> &req, Function<void()> &&complete) {
	/*if (_application->isMainThread()) {
		_device->setSwapchainRenderQueue(Rc<RenderQueue>(req), move(complete));
		if (_running.load()) {
			pushEvent(Event::SwapChainForceRecreate, Rc<Ref>(), data::Value());
		}
	} else {
		auto d = new Function<void()>(move(complete));
		_application->performOnMainThread([this, req, complete, d] {
			_device->setSwapchainRenderQueue(Rc<RenderQueue>(req), move(*d));
			if (_running.load()) {
				pushEvent(Event::SwapChainForceRecreate, Rc<Ref>(), data::Value());
			}
			delete d;
		}, this);
	}*/
}

/*void Loop::reset(bool best) {
	_device->reset(*_queue, best);
	pushEvent(Event::SwapChainRecreated, nullptr, data::Value());
}*/

void Loop::end(bool success) {
	pushEvent(EventName::Exit, nullptr, data::Value(success));
	_thread.join();
}

const Instance *Loop::getInstance() const {
	return _application->getGlInstance();
}

void Loop::performOnThread(const Function<void()> &func, Ref *target) {
	_queue->onMainThread(Rc<thread::Task>::create([func] (const thread::Task &, bool success) {
		if (success) { func(); }
	}, target));
}

void Loop::setInterval(const Rc<Swapchain> &ref, uint64_t iv) {
	pushEvent(EventName::UpdateFrameInterval, ref.get(), data::Value(iv));
}

bool Loop::isOnThread() const {
	return std::this_thread::get_id() == _thread.get_id();
}

void Loop::autorelease(Ref *ref) {
	if (std::this_thread::get_id() == _thread.get_id()) {
		_internal->autorelease->emplace_back(ref);
	}
}

bool Loop::pollEvents(std::unique_lock<std::mutex> &lock, PresentationData &data, Context &context) {
	bool timerPassed = false;
	data.now = platform::device::_clock();
	auto counter = _queue->getOutputCounter();
	if (!context.events->empty() || counter > 0) {
		// there are pending events, check if timeout already passed
		if (data.now - data.last > data.updateInterval) {
			timerPassed = true;
		}
	} else {
		if (data.now - data.last > data.updateInterval) {
			timerPassed = true;
		} else {
			if (_internal->timers->empty()) {
				// no timers - just wait for events with 60FPS wakeups
				auto t = std::max(data.updateInterval, uint64_t(1000000 / 60));
				_cond.wait_for(lock, std::chrono::microseconds(t), [&] {
					return !_internal->events->empty() || _queue->getOutputCounter() > 0;
				});
				context.events = _internal->events;
				_internal->events = _internal->eventsSwap;
				_internal->eventsSwap = context.events;
			} else {
				if (!_cond.wait_for(lock, std::chrono::microseconds(data.updateInterval - (data.now - data.last)), [&] {
					return !_internal->events->empty();
				})) {
					timerPassed = true;
				} else {
					context.events = _internal->events;
					_internal->events = _internal->eventsSwap;
					_internal->eventsSwap = context.events;
				}
			}
		}
	}
	lock.unlock();
	return timerPassed;
}

void Loop::runTimers(uint64_t dt, Context &t) {
	auto timers = _internal->timers;
	_internal->timers = _internal->reschedule;

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

	if (!_internal->timers->empty()) {
		for (auto &it : *_internal->timers) {
			timers->emplace_back(std::move(it));
		}
		_internal->timers->clear();
	}
	_internal->timers = timers;
}

}
