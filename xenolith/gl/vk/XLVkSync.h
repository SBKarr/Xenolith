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


#ifndef XENOLITH_GL_VK_XLVKSYNC_H_
#define XENOLITH_GL_VK_XLVKSYNC_H_

#include "XLVk.h"
#include "XLGlObject.h"

namespace stappler::xenolith::vk {

class Device;
class DeviceQueue;
class Loop;

/* VkSemaphore wrapper
 *
 * usage pattern:
 * - store handles in common storage
 * - pop one before running signal function
 * - run function, that signals VkSemaphore, handle should be acquired with getSemaphore()
 * - run function, that waits on VkSemaphore
 * - push Semaphore back into storage
 */

class Semaphore : public gl::Semaphore {
public:
	virtual ~Semaphore();

	bool init(Device &);

	VkSemaphore getSemaphore() const { return _sem; }

protected:
	using gl::Semaphore::init;

	VkSemaphore _sem = VK_NULL_HANDLE;
};


/* VkFence wrapper
 *
 * usage pattern:
 * - store handles in common storage
 * - pop one before running signal function
 * - associate resources with Fence
 * - run function, that signals VkFence
 * - schedule spinner on check()
 * - release resources when VkFence is signaled
 * - push Fence back into storage when VkFence is signaled
 * - storage should reset() Fence on push
 */

class Fence : public gl::Object {
public:
	enum State {
		Disabled,
		Armed,
		Signaled
	};

	virtual ~Fence();

	bool init(Device &);
	void clear();

	VkFence getFence() const { return _fence; }

	void setFrame(Function<bool()> &&schedule, Function<void()> &&release, uint64_t f);
	uint64_t getFrame() const { return _frame; }

	void setScheduleCallback(Function<bool()> &&schedule);
	void setReleaseCallback(Function<bool()> &&release);

	uint64_t getArmedTime() const { return _armedTime; }

	bool isArmed() const { return _state == Armed; }
	void setArmed(DeviceQueue &);
	void setArmed();

	void setTag(StringView);
	StringView getTag() const { return _tag; }

	// function will be called and ref will be released on fence's signal
	void addRelease(Function<void(bool)> &&, Ref *, StringView tag);

	bool schedule(Loop &);

	bool check(Loop &, bool lockfree = true);
	void reset(Loop &, Function<void(Rc<Fence> &&)> &&);

protected:
	using gl::Object::init;

	void scheduleReset(Loop &);
	void scheduleReleaseReset(Loop &, bool s);
	void doRelease(bool success);

	struct ReleaseHandle {
		Function<void(bool)> callback;
		Rc<Ref> ref;
		StringView tag;
	};

	uint64_t _frame = 0;
	State _state = Disabled;
	VkFence _fence = VK_NULL_HANDLE;
	Vector<ReleaseHandle> _release;
	Mutex _mutex;
	DeviceQueue *_queue = nullptr;
	uint64_t _armedTime = 0;
	StringView _tag;

	Function<bool()> _scheduleFn;
	Function<void()> _releaseFn;
};

}

#endif /* XENOLITH_GL_VK_XLVKSYNC_H_ */
