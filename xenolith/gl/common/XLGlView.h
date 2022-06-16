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

#ifndef XENOLITH_GL_COMMON_XLGLVIEW_H_
#define XENOLITH_GL_COMMON_XLGLVIEW_H_

#include "XLRenderQueueFrameEmitter.h"
#include "XLEventHeader.h"

namespace stappler::xenolith::gl {

class Device;
class Loop;
class Instance;

class View : public thread::ThreadInterface<Interface> {
public:
	using AttachmentLayout = renderqueue::AttachmentLayout;
	using ImageStorage = renderqueue::ImageStorage;
	using FrameEmitter = renderqueue::FrameEmitter;
	using FrameRequest = renderqueue::FrameRequest;
	using RenderQueue = renderqueue::Queue;

	static EventHeader onScreenSize;

	View();
	virtual ~View();

	virtual bool init(Loop &, ViewInfo &&);

	virtual void run() = 0;
	virtual void runWithQueue(const Rc<renderqueue::Queue> &) = 0;
	virtual void end();

	virtual void update();
	virtual void close();

	void performOnThread(Function<void()> &&func, Ref *target = nullptr, bool immediate = false);

	// true - if presentation request accepted, false otherwise,
	// frame should not mark image as detached if false is returned
	virtual bool present(Rc<ImageStorage> &&) = 0;

	// present image in place instead of scheduling presentation
	// should be called in view's thread
	virtual bool presentImmediate(Rc<ImageStorage> &&) = 0;

	// invalidate swapchain image target, if drawing process was not successful
	virtual void invalidateTarget(Rc<ImageStorage> &&) = 0;

	const Rc<Director> &getDirector() const { return _director; }
	const Rc<Loop> &getLoop() const { return _loop; }

	// returns current screen extent, non thread-safe, used in view's thread
	// for main thread, use Director::getScreenExtent
	virtual const Extent2 & getScreenExtent() const { return _screenExtent; }

	// update screen extent, non thread-safe
	// only updates field, view is not resized
	virtual void setScreenExtent(Extent2);

	// handle and propagate input event
	void handleInputEvent(const InputEventData &);

	virtual void runFrame(const Rc<RenderQueue> &, Extent2);

	virtual ImageInfo getSwapchainImageInfo() const;
	virtual ImageInfo getSwapchainImageInfo(const SwapchainConfig &cfg) const;
	virtual ImageViewInfo getSwapchainImageViewInfo(const ImageInfo &image) const;

	// interval between two frame presented
	uint64_t getLastFrameInterval() const;
	uint64_t getAvgFrameInterval() const;

	// time between frame stared and last queue submission completed
	uint64_t getLastFrameTime() const;
	uint64_t getAvgFrameTime() const;

	float getDensity() const { return _density; }
	ScreenOrientation getScreenOrientation() const { return _orientation; }

	bool isTouchDevice() const { return _isTouchDevice; }
	bool hasFocus() const { return _hasFocus; }
	bool isInBackground() const { return _inBackground; }
	bool isPointerWithinWindow() const { return _pointerInWindow; }

protected:
	virtual void wakeup() = 0;

	Extent2 _screenExtent;

	float _density = 1.0f;

	ScreenOrientation _orientation = ScreenOrientation::Landscape;

	bool _isTouchDevice = false;
	bool _inBackground = false;
	bool _hasFocus = true;
	bool _pointerInWindow = false;
	std::atomic<bool> _running = false;

	Rc<Director> _director;
	Rc<Loop> _loop;
	Rc<renderqueue::FrameEmitter> _frameEmitter;

	Function<SwapchainConfig(const SurfaceInfo &)> _selectConfig;
	Function<void(const Rc<Director> &)> _onCreated;
	Function<void()> _onClosed;

	uint64_t _gen = 1;
	gl::SwapchainConfig _config;

	std::thread _thread;
	std::thread::id _threadId;
	std::atomic_flag _shouldQuit;
	Mutex _mutex;
	Vector<Pair<Function<void()>, Rc<Ref>>> _callbacks;

	uint64_t _frameInterval = 1'000'000 / 60; // in microseconds
	mutable Mutex _frameIntervalMutex;
	uint64_t _lastFrameStart = 0;
	uint64_t _lastFrameInterval = 0;
	math::MovingAverage<15, uint64_t> _avgFrameInterval;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLVIEW_H_ */
