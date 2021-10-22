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

#include "XLGlSwapchain.h"
#include "XLGlFrame.h"

namespace stappler::xenolith::gl {

Swapchain::~Swapchain() {
	if (_renderQueue) {
		_renderQueue->disable();
	}
	_renderQueue = nullptr;
	_nextRenderQueue = nullptr;
}

bool Swapchain::init(const View *view, const Rc<RenderQueue> &queue) {
	_view = view;
	_renderQueue = queue;
	if (_renderQueue) {
		_renderQueue->enable(this);
	}
	return true;
}

bool Swapchain::recreateSwapchain(Device &, gl::SwapchanCreationMode) {
	return false;
}

void Swapchain::invalidate(Device &) {

}

Rc<gl::FrameHandle> Swapchain::beginFrame(gl::Loop &loop, bool force) {
	if (force) {
		_frame = 0;
	}

	if (!canStartFrame()) {
		scheduleNextFrame();
		if (force) {
			log::text("gl::Swapchain", "Forced frame failed");
		}
		return nullptr;
	}

	_nextFrameScheduled = false;
	auto frame = makeFrame(loop, _frames.empty());
	if (frame && frame->isValidFlag()) {
		_view->pushEvent(AppEvent::Update);
		frame->update(true);
		if (frame->isValidFlag()) {
			_frames.push_back(frame);
			_frame = platform::device::_clock();
			return frame;
		}
	}
	if (force) {
		log::text("gl::Swapchain", "Forced frame failed");
	}
	return nullptr;
}

void Swapchain::setFrameSubmitted(Rc<gl::FrameHandle> frame) {
	auto it = _frames.begin();
	while (it != _frames.end()) {
		if ((*it) == frame) {
			it = _frames.erase(it);
		} else {
			++ it;
		}
	}

	for (auto &it : _frames) {
		if (!it->isReadyForSubmit()) {
			it->setReadyForSubmit(true);
			break;
		}
	}

	++ _submitted;

	if (_nextFrameScheduled) {
		frame->getLoop()->pushEvent(Loop::EventName::FrameTimeoutPassed, this);
	}
}

void Swapchain::invalidateFrame(gl::FrameHandle &handle) {
	handle.invalidate();
	auto it = std::find(_frames.begin(), _frames.end(), &handle);
	if (it != _frames.end()) {
		_frames.erase(it);
	}
}

bool Swapchain::isFrameValid(const gl::FrameHandle &handle) {
	if (handle.getGen() == _gen && std::find(_frames.begin(), _frames.end(), &handle) != _frames.end()) {
		return true;
	}
	return false;
}

void Swapchain::incrementGeneration(AppEvent::Value event) {
	_valid = false;
	if (_submitted == 0) {
		log::text("gl::Swapchain", "Increment generation without submitted frames");
	}
	++ _gen;
	if (!_frames.empty()) {
		auto f = move(_frames);
		_frames.clear();
		for (auto &it : f) {
			invalidateFrame(*it);
		}
	}
	_suboptimal = 20;
	if (event) {
		_view->pushEvent(event);
	}
	_submitted = 0;
}

bool Swapchain::isResetRequired() {
	if (_suboptimal > 0) {
		-- _suboptimal;
		if (_suboptimal == 0) {
			if (!isBestPresentMode()) {
				return true;
			}
		}
	}
	return false;
}

bool Swapchain::canStartFrame() const {
	if (!_valid) {
		return false;
	}

	if (_frames.size() >= 2) {
		return false;
	}

	for (auto &it : _frames) {
		if (!it->isSubmitted()) {
			return false;
		}
	}

	return true;
}

bool Swapchain::scheduleNextFrame() {
	auto prev = _nextFrameScheduled;
	_nextFrameScheduled = true;
	return prev;
}

}
