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

#ifndef XENOLITH_GL_COMMON_XLGLLOOP_H_
#define XENOLITH_GL_COMMON_XLGLLOOP_H_

#include "XLGl.h"
#include "XLGlDevice.h"
#include "XLGlView.h"

namespace stappler::xenolith::gl {

class Loop : public thread::ThreadHandlerInterface {
public:
	struct Event final {
		enum EventName {
			Update, // force-update
			SwapChainDeprecated, // swapchain was deprecated by view
			SwapChainRecreated, // swapchain was recreated by view
			SwapChainForceRecreate, // force engine to recreate swapchain with best params
			FrameUpdate,
			FrameSubmitted,
			FrameTimeoutPassed, // framerate heartbeat
			// FramePresentReady, // frame ready for presentation
			UpdateFrameInterval, // view wants us to update frame interval
			CompileResource, // new GL resource requested
			Exit,
		};

		Event(EventName event, Rc<Ref> &&data, data::Value &&value)
		: event(event), data(move(data)), value(move(value)) { }

		EventName event = EventName::Update;
		Rc<Ref> data;
		data::Value value;
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

	Loop(Application *, const Rc<View> &, const Rc<Device> &, const Rc<Director> &, uint64_t frameMicroseconds);
	virtual ~Loop();

	virtual void threadInit() override; // called when thread is created
	virtual bool worker() override;

	void pushEvent(Event::EventName, Rc<Ref> && = Rc<Ref>(), data::Value && = data::Value());
	void pushContextEvent(Event::EventName, Rc<Ref> && = Rc<Ref>(), data::Value && = data::Value());

	void schedule(Function<bool(Context &)> &&);
	void schedule(Function<bool(Context &)> &&, uint64_t);

	void begin();
	void end(bool success = true);

	std::thread::id getThreadId() const { return _threadId; }
	const Rc<Device> &getDevice() const { return _device; }
	Application *getApplication() const { return _application; }
	const Rc<thread::TaskQueue> &getQueue() const { return _queue; }
	uint64_t getClock() const { return _clock; }

	void performOnThread(const Function<void()> &func, Ref *target = nullptr);

	// common shortcuts
	void setInterval(uint64_t iv) {
		pushEvent(Event::UpdateFrameInterval, nullptr, data::Value(iv));
	}

	void recreateSwapChain() {
		pushEvent(Event::SwapChainDeprecated, nullptr, data::Value());
	}

	void compileResource(Rc<Resource> req) {
		pushEvent(Event::CompileResource, req, data::Value());
	}

	void reset() {
		pushEvent(Event::SwapChainRecreated, nullptr, data::Value());
	}

	bool isOnThread() const;

	void autorelease(Ref *);

protected:
	struct Internal;

	void runTimers(uint64_t dt, Context &t);

	Context *_currentContext = nullptr;

	Application *_application = nullptr;
	Rc<View> _view;
	Rc<Device> _device;
	Rc<Director> _director;

	std::atomic<uint64_t> _frameTimeMicroseconds = 1000'000 / 60;
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
};

}

#endif /* XENOLITH_GL_COMMON_XLGLLOOP_H_ */
