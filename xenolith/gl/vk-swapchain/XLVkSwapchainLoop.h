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

#ifndef XENOLITH_GL_VK_SWAPCHAIN_XLVKSWAPCHAINLOOP_H_
#define XENOLITH_GL_VK_SWAPCHAIN_XLVKSWAPCHAINLOOP_H_

#include "XLVk.h"
#include "XLGlInstance.h"

namespace stappler::xenolith::vk {

class Device;

class SwapchainLoop : public thread::ThreadInterface<Interface> {
public:
	static constexpr uint32_t LoopThreadId = 3;

	enum class EventName {
		Update, // force-update
		FrameUpdate,
		FrameInvalidated,
		CompileResource,
		CompileMaterials,
		RunRenderQueue,
		Exit,
	};

	struct Event final {
		Event(EventName event, Rc<Ref> &&data, Value &&value, Function<void(bool)> &&cb = nullptr)
		: event(event), data(move(data)), value(move(value)), callback(cb) { }

		EventName event = EventName::Update;
		Rc<Ref> data;
		Value value;
		Function<void(bool)> callback;
	};

	struct Context final {
		memory::vector<Event> *events;
		SwapchainLoop *loop;
	};

	struct Timer final {
		Timer(uint64_t interval, Function<bool(Context &)> &&cb, StringView t)
		: interval(interval), callback(move(cb)), tag(t) { }

		uint64_t interval;
		uint64_t value = 0;
		Function<bool(Context &)> callback; // return true if timer is complete and should be removed
		StringView tag;
	};

	virtual ~SwapchainLoop();

	SwapchainLoop();

	bool init(Rc<gl::Instance> &&, uint32_t deviceIdx = gl::Instance::DefaultDevice);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

protected:
	struct Internal;

	std::thread _thread;
	std::thread::id _threadId;
	Internal *_internal = nullptr;
	uint32_t _deviceIndex = gl::Instance::DefaultDevice;

	Rc<gl::Instance> _instance;

	Mutex _pendingEventMutex;
	Vector<Event> _pendingEvents;
};

}

#endif /* XENOLITH_GL_VK_SWAPCHAIN_XLVKSWAPCHAINLOOP_H_ */
