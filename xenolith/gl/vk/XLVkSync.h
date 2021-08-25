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

/* VkSemaphore wrapper
 *
 * usage pattern:
 * - store handles in common storage
 * - pop one before running signal function
 * - run function, that signals VkSemaphore, handle should be acquired with getUnsignalled()
 * - run function, that waits on VkSemaphore
 * - push Semaphore back into storage
 */

class Semaphore : public gl::Object {
public:
	virtual ~Semaphore();

	bool init(Device &);

	void setSignaled(bool);
	bool isSignaled() const { return _signaled; }
	VkSemaphore getSemaphore() const { return _sem; }
	VkSemaphore getUnsignalled();
	void reset();

protected:
	bool _signaled = false;
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
	virtual ~Fence();

	bool init(Device &);

	bool isSignaled() const { return _signaled; }
	VkFence getFence() const { return _fence; }

	// function will be called and ref will be released on fence's signal
	void addRelease(Function<void()> &&, Ref * = nullptr);

	bool check();
	void reset();

protected:
	struct ReleaseHandle {
		Function<void()> callback;
		Rc<Ref> ref;
	};

	bool _signaled = false;
	VkFence _fence = VK_NULL_HANDLE;
	Vector<ReleaseHandle> _release;
};

}

#endif /* XENOLITH_GL_VK_XLVKSYNC_H_ */
