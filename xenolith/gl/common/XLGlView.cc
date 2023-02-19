/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLGlView.h"
#include "XLInputDispatcher.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLGlLoop.h"
#include "XLGlDevice.h"
#include "XLRenderQueueFrameHandle.h"

namespace stappler::xenolith::gl {

XL_DECLARE_EVENT_CLASS(View, onFrameRate);

View::View() { }

View::~View() { }

bool View::init(Loop &loop, ViewInfo &&info) {
	_loop = &loop;
	_constraints.extent = Extent2(info.rect.width, info.rect.height);
	if (info.density != 0.0f) {
		_constraints.density = info.density;
	}
	_frameEmitter = Rc<FrameEmitter>::create(_loop, info.frameInterval);
	_selectConfig = move(info.config);
	_onCreated = move(info.onCreated);
	_onClosed = move(info.onClosed);
	return true;
}

void View::end() {
	_running = false;
	_frameEmitter->invalidate();
	_loop->removeView(this);
	_loop->getApplication()->performOnMainThread([this, cb = move(_onClosed)] () {
		if (_director) {
			_director->end();
		}
		cb();
	}, this);
}

void View::update(bool displayLink) {
	Vector<Pair<Function<void()>, Rc<Ref>>> callback;

	_mutex.lock();
	callback = move(_callbacks);
	_callbacks.clear();
	_mutex.unlock();

	for (auto &it : callback) {
		it.first();
	}
}

void View::close() {
	_shouldQuit.clear();
}

void View::performOnThread(Function<void()> &&func, Ref *target, bool immediate) {
	if (immediate && std::this_thread::get_id() == _threadId) {
		func();
	} else {
		std::unique_lock<Mutex> lock(_mutex);
		if (_running) {
			_callbacks.emplace_back(move(func), target);
			wakeup();
		}
	}
}

void View::handleInputEvent(const InputEventData &event) {
	_loop->getApplication()->performOnMainThread([this, event = event] () mutable {
		if (event.isPointEvent()) {
			event.point.density = _constraints.density;
		}

		switch (event.event) {
		case InputEventName::Background:
			_inBackground = event.getValue();
			break;
		case InputEventName::PointerEnter:
			_pointerInWindow = event.getValue();
			break;
		case InputEventName::FocusGain:
			_hasFocus = event.getValue();
			break;
		default:
			break;
		}
		_director->getInputDispatcher()->handleInputEvent(event);
	}, this);
	setReadyForNextFrame();
}

void View::handleInputEvents(Vector<InputEventData> &&events) {
	_loop->getApplication()->performOnMainThread([this, events = move(events)] () mutable {
		for (auto &event : events) {
			if (event.isPointEvent()) {
				event.point.density = _constraints.density;
			}

			switch (event.event) {
			case InputEventName::Background:
				_inBackground = event.getValue();
				break;
			case InputEventName::PointerEnter:
				_pointerInWindow = event.getValue();
				break;
			case InputEventName::FocusGain:
				_hasFocus = event.getValue();
				break;
			default:
				break;
			}
			_director->getInputDispatcher()->handleInputEvent(event);
		}
	}, this);
	setReadyForNextFrame();
}

/*void View::runFrame(const Rc<RenderQueue> &queue) {
	auto a = queue->getPresentImageOutput();
	if (!a) {
		a = queue->getTransferImageOutput();
	}
	if (!a) {
		log::vtext("vk::View", "Fail to run view with queue '", queue->getName(),  "': no usable output attachments found");
		return;
	}

	auto req = Rc<FrameRequest>::create(queue, _frameEmitter, _constraints);
	req->setOutput(Rc<renderqueue::FrameOutputBinding>::create(a, this));
	_frameEmitter->submitNextFrame(move(req));
}*/

gl::ImageInfo View::getSwapchainImageInfo() const {
	return getSwapchainImageInfo(_config);
}

gl::ImageInfo View::getSwapchainImageInfo(const gl::SwapchainConfig &cfg) const {
	gl::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = gl::ImageFlags::None;
	swapchainImageInfo.imageType = gl::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = gl::ArrayLayers( 1 );
	swapchainImageInfo.usage = gl::ImageUsage::ColorAttachment;
	if (cfg.transfer) {
		swapchainImageInfo.usage |= gl::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

gl::ImageViewInfo View::getSwapchainImageViewInfo(const gl::ImageInfo &image) const {
	gl::ImageViewInfo info;
	switch (image.imageType) {
	case gl::ImageType::Image1D:
		info.type = gl::ImageViewType::ImageView1D;
		break;
	case gl::ImageType::Image2D:
		info.type = gl::ImageViewType::ImageView2D;
		break;
	case gl::ImageType::Image3D:
		info.type = gl::ImageViewType::ImageView3D;
		break;
	}

	return image.getViewInfo(info);
}

uint64_t View::getLastFrameInterval() const {
	return _lastFrameInterval;
}
uint64_t View::getAvgFrameInterval() const {
	return _avgFrameIntervalValue;
}

uint64_t View::getLastFrameTime() const {
	return _frameEmitter->getLastFrameTime();
}
uint64_t View::getAvgFrameTime() const {
	return _frameEmitter->getAvgFrameTime();
}

uint64_t View::getAvgFenceTime() const {
	return _frameEmitter->getAvgFenceTime();
}

uint64_t View::getFrameInterval() const {
	std::unique_lock<Mutex> lock(_frameIntervalMutex);
	return _frameInterval;
}

void View::setFrameInterval(uint64_t value) {
	performOnThread([this, value] {
		std::unique_lock<Mutex> lock(_frameIntervalMutex);
		_frameInterval = value;
		onFrameRate(this, int64_t(_frameInterval));
	}, this, true);
}

void View::setNavigationEmpty(bool value) {
	performOnThread([this, value] {
		_navigationEmpty = value;
	}, this, true);
}

void View::setReadyForNextFrame() {

}

}
