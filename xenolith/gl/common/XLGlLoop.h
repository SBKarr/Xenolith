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

#ifndef XENOLITH_GL_COMMON_XLGLLOOP_H_
#define XENOLITH_GL_COMMON_XLGLLOOP_H_

#include "XLGl.h"
#include "XLGlDevice.h"
#include "XLGlView.h"
#include "XLGlSwapchain.h"

namespace stappler::xenolith::gl {

struct PresentationData;

class Loop : public thread::ThreadHandlerInterface {
public:
	static constexpr uint32_t LoopThreadId = 2;

	enum class EventName {
		Update, // force-update
		SwapChainDeprecated, // swapchain was deprecated by view
		SwapChainRecreated, // swapchain was recreated by view
		SwapChainForceRecreate, // force engine to recreate swapchain with best params
		FrameUpdate,
		FrameSubmitted,
		FrameInvalidated,
		FrameTimeoutPassed,
		UpdateFrameInterval, // view wants us to update frame interval
		CompileResource,
		CompileMaterials,
		RunRenderQueue,
		Exit,
	};

	struct Event final {
		Event(EventName event, Rc<Ref> &&data, data::Value &&value, Function<void(bool)> &&cb = nullptr)
		: event(event), data(move(data)), value(move(value)), callback(cb) { }

		EventName event = EventName::Update;
		Rc<Ref> data;
		data::Value value;
		Function<void(bool)> callback;
	};

	struct Context final {
		memory::vector<Event> *events;
	};

	struct Timer final {
		Timer(uint64_t interval, Function<bool(Context &)> &&cb)
		: interval(interval), callback(move(cb)) { }

		uint64_t interval;
		uint64_t value = 0;
		Function<bool(Context &)> callback; // return true if timer is complete and should be removed
	};

	Loop(Application *, const Rc<Device> &);
	virtual ~Loop();

	virtual void threadInit() override; // called when thread is created
	virtual bool worker() override;

	void pushEvent(EventName, Rc<Ref> && = Rc<Ref>(), data::Value && = data::Value(), Function<void(bool)> && = nullptr);
	void pushContextEvent(EventName, Rc<Ref> && = Rc<Ref>(), data::Value && = data::Value(), Function<void(bool)> && = nullptr);

	void schedule(Function<bool(Context &)> &&);
	void schedule(Function<bool(Context &)> &&, uint64_t);

	void begin();
	void end(bool success = true);

	std::thread::id getThreadId() const { return _threadId; }
	const Rc<Device> &getDevice() const { return _device; }
	const Application *getApplication() const { return _application; }
	const Instance *getInstance() const;
	const Rc<thread::TaskQueue> &getQueue() const { return _queue; }
	uint64_t getClock() const { return _clock; }
	bool isRunning() const { return _running.load(); }

	void performOnThread(const Function<void()> &func, Ref *target = nullptr);

	void setInterval(const Rc<Swapchain> &, uint64_t iv);

	void recreateSwapChain(const Rc<Swapchain> &ref) {
		pushEvent(EventName::SwapChainDeprecated, ref.get(), data::Value());
	}

	void compileResource(const Rc<Resource> &req) {
		pushEvent(EventName::CompileResource, req.get(), data::Value());
	}

	void compileMaterials(const Rc<MaterialInputData> &req) {
		pushEvent(EventName::CompileMaterials, req.get(), data::Value());
	}

	// if called before loop is started - should compile RenderQueue in place
	// if not - compilation is scheduled on TaskQueue
	void compileRenderQueue(const Rc<RenderQueue> &req, Function<void(bool)> && = nullptr);

	void compileImage(const Rc<DynamicImage> &, Function<void(bool)> && = nullptr);

	// run frame with RenderQueue
	void runRenderQueue(const Rc<RenderQueue> &req,
			uint64_t gen = 0, Function<void(bool)> && = nullptr);

	void runRenderQueue(const Rc<RenderQueue> &req, Map<const Attachment *, Rc<AttachmentInputData>> &&,
			uint64_t gen = 0, Function<void(bool)> && = nullptr);

	bool isOnThread() const;

	void autorelease(Ref *);

protected:
	struct Internal;

	virtual bool pollEvents(std::unique_lock<std::mutex> &lock, PresentationData &data, Context &context);

	void runTimers(uint64_t dt, Context &t);

	Context *_currentContext = nullptr;

	const Application *_application = nullptr;
	Rc<Device> _device;

	std::atomic<uint64_t> _rate;
	std::thread _thread;
	std::thread::id _threadId;

	memory::pool_t *_pool = nullptr;
	Internal *_internal = nullptr;

	std::atomic<bool> _running = false;
	std::mutex _mutex;
	std::condition_variable _cond;

	Rc<thread::TaskQueue> _queue;
	uint64_t _clock = 0;

	Vector<Event> _pendingEvents;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLLOOP_H_ */
