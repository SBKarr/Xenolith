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

#include "XLVkSwapchainLoop.h"
#include "XLGlDevice.h"

namespace stappler::xenolith::vk {

struct SwapchainLoop::Internal : memory::AllocPool {
	Internal(memory::pool_t *pool) : pool(pool) {
		events = new (pool) memory::vector<Event>(); events->reserve(4);
		timers = new (pool) memory::vector<Timer>(); timers->reserve(8);
		reschedule = new (pool) memory::vector<Timer>(); reschedule->reserve(8);
		autorelease = new (pool) memory::vector<Rc<Ref>>(); autorelease->reserve(8);
	}

	void waitIdle();

	memory::pool_t *pool = nullptr;
	memory::vector<Event> *events;
	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
	memory::vector<Rc<Ref>> *autorelease;

	Rc<gl::TaskQueue> queue;
	Rc<gl::Device> device;
};

SwapchainLoop::~SwapchainLoop() { }

SwapchainLoop::SwapchainLoop() { }

bool SwapchainLoop::init(Rc<gl::Instance> &&instance, uint32_t deviceIdx) {
	_instance = move(instance);
	_deviceIndex = deviceIdx;
	_thread = std::thread(SwapchainLoop::workerThread, this, nullptr);
	return true;
}

void SwapchainLoop::threadInit() {
	thread::ThreadInfo::setThreadInfo("SwapchainLoop");
	_threadId = std::this_thread::get_id();

	memory::pool::initialize();
	auto pool = memory::pool::createTagged("SwapchainLoop", mempool::custom::PoolFlags::ThreadSafeAllocator);

	memory::pool::push(pool);

	_internal = new (pool) Internal(pool);
	_internal->pool = pool;

	_internal->device = _instance->makeDevice(_deviceIndex);

	_internal->queue = Rc<gl::TaskQueue>::alloc("Gl::Loop::Queue");
	_internal->queue->spawnWorkers(gl::TaskQueue::Flags::Waitable | gl::TaskQueue::Flags::Cancelable, LoopThreadId,
			math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16)));

	memory::pool::pop();
}

void SwapchainLoop::threadDispose() {
	auto pool = _internal->pool;

	memory::pool::push(pool);

	_internal->queue->lock();

	_internal->waitIdle();

	_internal->queue->unlock();

	_internal->queue->waitForAll();

	_internal->queue->lock();
	_internal->device->end();
	_internal->queue->unlock();

	_internal->queue->waitForAll();

	_internal->queue->lock();
	_internal->events->clear();
	_internal->timers->clear();
	_internal->reschedule->clear();
	_internal->autorelease->clear();
	_internal = nullptr;
	_internal->queue->unlock();

	_pendingEventMutex.lock();
	_pendingEvents.clear();
	_pendingEventMutex.unlock();

	_internal->queue->cancelWorkers();

	delete _internal;
	_internal = nullptr;

	memory::pool::pop();
	memory::pool::destroy(pool);
}

bool SwapchainLoop::worker() {

}

}
