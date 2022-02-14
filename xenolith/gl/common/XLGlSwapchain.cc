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

#include "XLGlSwapchain.h"

#include "XLGlFrameHandle.h"

namespace stappler::xenolith::gl {

Swapchain::~Swapchain() { }

bool Swapchain::init(const View *view, Function<SwapchainConfig(const SurfaceInfo &)> &&cb) {
	_view = view;
	_configCallback = move(cb);
	return true;
}

bool Swapchain::recreateSwapchain(Device &, gl::SwapchanCreationMode) {
	return false;
}

void Swapchain::invalidate(Device &) {

}

void Swapchain::incrementGeneration(AppEvent::Value event) {
	_valid = false;
	/*if (_submitted == 0) {
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
	_submitted = 0;*/
}

bool Swapchain::isResetRequired() {
	/*if (_suboptimal > 0) {
		-- _suboptimal;
		if (_suboptimal == 0) {
			if (!isBestPresentMode()) {
				return true;
			}
		}
	}*/
	return false;
}

}
