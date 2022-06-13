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

#include "XLVkLoop.h"
#include "XLVkInstance.h"
#include "XLEventLoop.h"
#include "XLVkView.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"

#include "XLVkTransferQueue.h"
#include "XLVkMaterialCompiler.h"
#include "XLVkRenderQueueCompiler.h"

namespace stappler::xenolith::vk {

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
};

struct Loop::Internal final : memory::AllocPool {
	Internal(memory::pool_t *pool, Loop *l) : pool(pool), loop(l) {
		timers = new (pool) memory::vector<Timer>(); timers->reserve(8);
		reschedule = new (pool) memory::vector<Timer>(); reschedule->reserve(8);
		autorelease = new (pool) memory::vector<Rc<Ref>>(); autorelease->reserve(8);
	}

	void setDevice(Rc<Device> &&dev) {
		requiredTasks += 3;

		device = move(dev);

		device->begin(*loop, *queue, [this] (bool success) {
			auto resources = loop->getResourceCache();
			auto texSet = device->getTextureSetLayout();
			loop->getApplication()->performOnMainThread([texSet, resources, loop = loop] {
				resources->addImage(gl::ImageData::make(texSet->getEmptyImageObject()));
				resources->addImage(gl::ImageData::make(texSet->getSolidImageObject()));
			}, loop);

			onInitTaskPerformed(success, "DeviceResources");
		});

		renderQueueCompiler = Rc<RenderQueueCompiler>::create(*device);
		materialQueue = Rc<MaterialCompiler>::create();
		transferQueue = Rc<TransferQueue>::create();

		compileRenderQueue(materialQueue, [this] (bool success) {
			onInitTaskPerformed(success, "MaterialQueue");
		});
		compileRenderQueue(transferQueue, [this] (bool success) {
			onInitTaskPerformed(success, "TransferQueue");
		});
	}

	void endDevice() {
		fences.clear();
		transferQueue = nullptr;
		materialQueue->clearRequests();
		materialQueue = nullptr;
		renderQueueCompiler = nullptr;
		device->end();

		loop->getApplication()->performOnMainThread([res = loop->getResourceCache()] {
			res->invalidate();
		}, device);

		device = nullptr;
	}

	void waitIdle() {
		auto r = running->exchange(false);

		queue->lock(); // lock to prevent new tasks

		// wait for all pending tasks on fences
		for (auto &it : scheduledFences) {
			it->check(*loop, false);
		}
		scheduledFences.clear();

		// wait for device
		device->getTable()->vkDeviceWaitIdle(device->getDevice());

		queue->unlock();

		// wait for all CPU tasks
		queue->waitForAll();

		// after this, there should be no CPU or GPU tasks pending or executing

		if (r) {
			running->exchange(true);
		}
	}

	void compileRenderQueue(const Rc<renderqueue::Queue> &req, Function<void(bool)> &&cb) {
		auto input = Rc<RenderQueueInput>::alloc();
		input->queue = req;

		auto h = Rc<DeviceFrameHandle>::create(*loop, *device, renderQueueCompiler->makeRequest(move(input)), 0);
		if (cb) {
			h->setCompleteCallback([cb] (FrameHandle &handle) {
				cb(handle.isValid());
			});
		}

		h->update(true);
	}

	void compileMaterials(Rc<gl::MaterialInputData> &&req) {
		if (materialQueue->inProgress(req->attachment)) {
			materialQueue->appendRequest(req->attachment, move(req));
		} else {
			auto attachment = req->attachment;
			materialQueue->setInProgress(attachment);
			materialQueue->runMaterialCompilationFrame(*loop, move(req));
		}
	}

	void compileResource(Rc<gl::Resource> &&req) {
		auto h = loop->makeFrame(transferQueue->makeRequest(Rc<TransferResource>::create(device->getAllocator(), req)), 0);
		h->update(true);
	}

	void addView(gl::ViewInfo &&info) {
		auto view = platform::graphic::createView(*loop, *device, move(info));
		views.emplace(view);
	}

	void removeView(gl::View *view) {
		views.erase(view);
		((View *)view)->onRemoved();
	}

	void onInitTaskPerformed(bool success, StringView view) {
		if (success) {
			-- requiredTasks;
			if (requiredTasks == 0 && signalInit) {
				signalInit();
			}
		} else {
			log::vtext("Loop", "Fail to initalize: ", view);
		}
	}

	memory::pool_t *pool = nullptr;
	Loop *loop = nullptr;

	memory::vector<Timer> *timers;
	memory::vector<Timer> *reschedule;
	memory::vector<Rc<Ref>> *autorelease;

	Mutex resourceMutex;

	Rc<Device> device;
	Rc<gl::TaskQueue> queue;
	Vector<Rc<Fence>> fences;
	Set<Rc<Fence>> scheduledFences;
	Set<Rc<gl::View>> views;

	Rc<RenderQueueCompiler> renderQueueCompiler;
	Rc<TransferQueue> transferQueue;
	Rc<MaterialCompiler> materialQueue;
	std::atomic<bool> *running = nullptr;
	uint32_t requiredTasks = 0;

	Function<void()> signalInit;
};

struct Loop::Timer final {
	Timer(uint64_t interval, Function<bool(Loop &)> &&cb, StringView t)
	: interval(interval), callback(move(cb)), tag(t) { }

	uint64_t interval;
	uint64_t value = 0;
	Function<bool(Loop &)> callback; // return true if timer is complete and should be removed
	StringView tag;
};

Loop::~Loop() { }

Loop::Loop() { }

bool Loop::init(Application *app, Rc<Instance> &&instance, uint32_t deviceIdx) {
	if (!gl::Loop::init(app, instance)) {
		return false;
	}

	_vkInstance = move(instance);
	_deviceIndex = deviceIdx;

	_thread = std::thread(Loop::workerThread, this, nullptr);
	return true;
}

void Loop::waitRinning() {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_running.load()) {
		lock.unlock();
		_application->getQueue()->update(nullptr);
		return;
	}

	_cond.wait(lock);
	lock.unlock();
	_application->getQueue()->update(nullptr);
}

void Loop::threadInit() {
	thread::ThreadInfo::setThreadInfo("Gl::Loop");
	_threadId = std::this_thread::get_id();
	_shouldExit.test_and_set();

	memory::pool::initialize();
	auto pool = memory::pool::createTagged("Gl::Loop", mempool::custom::PoolFlags::ThreadSafeAllocator);

	memory::pool::push(pool);

	_internal = new (pool) vk::Loop::Internal(pool, this);
	_internal->pool = pool;
	_internal->running = &_running;

	_internal->signalInit = [this] {
		// signal to main thread
		_mutex.lock();
		_running.store(true);
		_cond.notify_all();
		_mutex.unlock();
	};

	_internal->queue = Rc<gl::TaskQueue>::alloc("Gl::Loop::Queue");
	_internal->queue->spawnWorkers(gl::TaskQueue::Flags::Cancelable | gl::TaskQueue::Flags::Waitable, LoopThreadId,
			config::getGlThreadCount());

	_internal->setDevice(_vkInstance->makeDevice(_deviceIndex));

	_frameCache = Rc<FrameCache>::create(*this, *_internal->device);

	memory::pool::pop();
}

void Loop::threadDispose() {
	auto pool = _internal->pool;

	memory::pool::push(pool);

	_internal->waitIdle();

	_internal->queue->lock();
	_internal->endDevice();
	_internal->queue->unlock();

	_internal->queue->waitForAll();

	_internal->queue->lock();
	_internal->timers->clear();
	_internal->reschedule->clear();
	_internal->autorelease->clear();
	_internal->queue->unlock();

	_internal->queue->cancelWorkers();

	_frameCache->invalidate();

	delete _internal;
	_internal = nullptr;

	memory::pool::pop();
	memory::pool::destroy(pool);
}

static bool Loop_pollEvents(Loop::Internal *internal, PresentationData &data) {
	bool timeoutPassed = false;
	auto counter = internal->queue->getOutputCounter();
	if (counter > 0) {
		XL_PROFILE_BEGIN(queue, "gl::Loop::Queue", "queue", 500);
		internal->queue->update();
		XL_PROFILE_END(queue)

		data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
		if (data.now - data.last > data.updateInterval) {
			timeoutPassed = true;
		}
	} else {
		data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
		if (data.now - data.last > data.updateInterval) {
			timeoutPassed = true;
		} else {
			if (internal->timers->empty() && internal->scheduledFences.empty()) {
				// no timers - just wait for events with 60FPS wakeups
				auto t = std::max(data.updateInterval, uint64_t(1'000'000 / 60));
				internal->queue->wait(TimeInterval::microseconds(t));
			} else {
				if (!internal->queue->wait(TimeInterval::microseconds(data.updateInterval - (data.now - data.last)))) {
					data.now = platform::device::_clock(platform::device::ClockType::Monotonic);
					timeoutPassed = true;
				}
			}
		}
	}
	return timeoutPassed;
}

static void Loop_runFences(Loop::Internal *internal) {
	auto it = internal->scheduledFences.begin();
	while (it != internal->scheduledFences.end()) {
		if ((*it)->check(*internal->loop, true)) {
			it = internal->scheduledFences.erase(it);
		}
	}
}

static void Loop_runTimers(Loop::Internal *internal, uint64_t dt) {
	auto timers = internal->timers;
	internal->timers = internal->reschedule;

	auto it = timers->begin();
	while (it != timers->end()) {
		if (it->interval) {
			it->value += dt;
			if (it->value > it->interval) {

				XL_PROFILE_BEGIN(timers, "gl::Loop::Timers", it->tag, 1000);

				auto ret = it->callback(*internal->loop);

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

			auto ret = it->callback(*internal->loop);

			XL_PROFILE_END(timers);

			if (ret) {
				it = timers->erase(it);
			} else {
				++ it;
			}
		}
	}

	if (!internal->timers->empty()) {
		for (auto &it : *internal->timers) {
			timers->emplace_back(std::move(it));
		}
		internal->timers->clear();
	}
	internal->timers = timers;
}

bool Loop::worker() {
	PresentationData data;

	auto pool = memory::pool::create(_internal->pool);

	while (_shouldExit.test_and_set()) {
		bool timeoutPassed = false;

		++ _clock;

		XL_PROFILE_BEGIN(loop, "vk::Loop", "loop", 1000);

		XL_PROFILE_BEGIN(poll, "vk::Loop::Poll", "poll", 500);
		timeoutPassed = Loop_pollEvents(_internal, data);
		XL_PROFILE_END(poll)

		Loop_runFences(_internal);

		if (timeoutPassed) {
			auto dt = data.now - data.last;
			XL_PROFILE_BEGIN(timers, "vk::Loop::Timers", "timers", 500);
			Loop_runTimers(_internal, dt);
			XL_PROFILE_END(timers)
			data.last = data.now;
		}

		XL_PROFILE_BEGIN(autorelease, "vk::Loop::Autorelease", "autorelease", 500);
		_internal->autorelease->clear();
		XL_PROFILE_END(autorelease)

		XL_PROFILE_END(loop)
		memory::pool::clear(pool);
	}

	memory::pool::destroy(pool);
	return false;
}

void Loop::cancel() {
	_shouldExit.clear();
	_thread.join();
}

void Loop::compileResource(Rc<gl::Resource> &&req) {
	performOnGlThread([this, req = move(req)] () mutable {
		_internal->compileResource(move(req));
	}, this, true);
}

void Loop::compileMaterials(Rc<gl::MaterialInputData> &&req) {
	performOnGlThread([this, req = move(req)] () mutable {
		_internal->compileMaterials(move(req));
	}, this, true);
}

void Loop::compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> &&callback) {
	performOnGlThread([this, req, callback = move(callback)]() mutable {
		_internal->compileRenderQueue(req, move(callback));
	}, this, true);
}

void Loop::compileImage(const Rc<gl::DynamicImage> &img, Function<void(bool)> &&callback) {
	performOnGlThread([this, img, callback = move(callback)]() mutable {
		_internal->device->getTextureSetLayout()->compileImage(*_internal->device, *this, img, move(callback));
	}, this, true);
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&callback) {
	performOnGlThread([this, req = move(req), gen, callback = move(callback)]() mutable {
		auto frame = makeFrame(move(req), gen);
		if (callback) {
			frame->setCompleteCallback([callback = move(callback)] (FrameHandle &handle) {
				callback(handle.isValid());
			});
		}
		frame->update(true);
	}, this, true);
}

void Loop::schedule(Function<bool(gl::Loop &)> &&cb, StringView tag) {
	if (isOnGlThread()) {
		_internal->timers->emplace_back(0, move(cb), tag);
	} else {
		performOnGlThread([this, cb = move(cb), tag] () mutable {
			_internal->timers->emplace_back(0, move(cb), tag);
		});
	}
}

void Loop::schedule(Function<bool(gl::Loop &)> &&cb, uint64_t delay, StringView tag) {
	if (isOnGlThread()) {
		_internal->timers->emplace_back(delay, move(cb), tag);
	} else {
		performOnGlThread([this, cb = move(cb), delay, tag] () mutable {
			_internal->timers->emplace_back(delay, move(cb), tag);
		});
	}
}

void Loop::performInQueue(Rc<thread::Task> &&task) {
	if (!_internal) {
		return;
	}

	_internal->queue->perform(move(task));
}

void Loop::performInQueue(Function<void()> &&func, Ref *target) {
	if (!_internal) {
		return;
	}

	_internal->queue->perform(move(func), target);
}

void Loop::performOnGlThread(Function<void()> &&func, Ref *target, bool immediate) {
	if (!_internal) {
		return;
	}

	if (immediate) {
		if (isOnGlThread()) {
			func();
			return;
		}
	}
	_internal->queue->onMainThread(move(func), target);
}

void Loop::performOnGlThread(Rc<thread::Task> &&task) {
	if (!_internal) {
		return;
	}

	_internal->queue->onMainThread(move(task));
}

bool Loop::isOnGlThread() const {
	return _threadId == std::this_thread::get_id();
}

auto Loop::makeFrame(Rc<FrameRequest> &&req, uint64_t gen) -> Rc<FrameHandle> {
	if (_running.load()) {
		return Rc<DeviceFrameHandle>::create(*this, *_internal->device, move(req), gen);
	}
	return nullptr;
}

Rc<gl::Framebuffer> Loop::acquireFramebuffer(const PassData *data, SpanView<Rc<gl::ImageView>> views, Extent2 e) {
	return _frameCache->acquireFramebuffer(data, views, e);
}

void Loop::releaseFramebuffer(Rc<gl::Framebuffer> &&fb) {
	_frameCache->releaseFramebuffer(move(fb));
}

auto Loop::acquireImage(const ImageAttachment *a, Extent3 e) -> Rc<ImageStorage> {
	gl::ImageInfo info = a->getInfo();
	info.extent = e;
	if (a->isTransient()) {
		info.usage |= gl::ImageUsage::TransientAttachment;
	}

	Vector<gl::ImageViewInfo> views;
	for (auto &desc : a->getDescriptors()) {
		if (desc->getAttachment()->getType() == renderqueue::AttachmentType::Image) {
			auto imgDesc = (renderqueue::ImageAttachmentDescriptor *)desc.get();
			views.emplace_back(gl::ImageViewInfo(*imgDesc));
		}
	}

	return _frameCache->acquireImage(info, views);
}

void Loop::releaseImage(Rc<ImageStorage> &&image) {
	_frameCache->releaseImage(move(image));
}

void Loop::addView(gl::ViewInfo &&info) {
	performOnGlThread([this, info = move(info)] () mutable {
		_internal->addView(move(info));
	}, this, true);
}

void Loop::removeView(gl::View *view) {
	performOnGlThread([this, view] () {
		_internal->removeView(view);
	}, view, true);
}

Rc<gl::Semaphore> Loop::makeSemaphore() {
	return _internal->device->makeSemaphore();
}

const Vector<gl::ImageFormat> &Loop::getSupportedDepthStencilFormat() const {
	return _internal->device->getSupportedDepthStencilFormat();
}

Rc<Fence> Loop::acquireFence(uint32_t v, bool init) {
	auto initFence = [&] (const Rc<Fence> &fence) {
		if (!init) {
			return;
		}

		fence->setFrame([this, fence] () mutable {
			if (isOnGlThread()) {
				// schedule fence
				_internal->scheduledFences.emplace(move(fence));
				return true;
			} else {
				performOnGlThread([this, fence = move(fence)] () mutable {
					if (!fence->check(*this, true)) {
						return;
					}

					_internal->scheduledFences.emplace(move(fence));
				}, this, true);
				return true;
			}
		}, [this, fence] () mutable {
			fence->clear();
			std::unique_lock<Mutex> lock(_internal->resourceMutex);
			_internal->fences.emplace_back(move(fence));
		}, v);
	};

	std::unique_lock<Mutex> lock(_internal->resourceMutex);
	if (!_internal->fences.empty()) {
		auto ref = _internal->fences.back();
		_internal->fences.pop_back();
		initFence(ref);
		return ref;
	}
	lock.unlock();
	auto ref = Rc<Fence>::create(*_internal->device);
	initFence(ref);
	return ref;
}

}
