/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLDirector.h"
#include "XLGlView.h"
#include "XLGlDevice.h"
#include "XLScene.h"
#include "XLVertexArray.h"
#include "XLScheduler.h"

namespace stappler::xenolith {

Director::Director() { }

Director::~Director() { }

bool Director::init(Application *app, Rc<Scene> &&scene) {
	_application = app;
	_pool = Rc<PoolRef>::alloc();
	_nextScene = move(scene);
	_pool->perform([&] {
		_scheduler = Rc<Scheduler>::create();
		_inputDispatcher = Rc<InputDispatcher>::create();
	});
	_startTime = _application->getClock();
	_time.global = 0;
	_time.app = 0;
	_time.delta = 0;
	return true;
}

const Rc<ResourceCache> &Director::getResourceCache() const {
	return _application->getResourceCache();
}

gl::SwapchainConfig Director::selectConfig(const gl::SurfaceInfo &info) const {
	gl::SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(2), info.minImageCount);
	ret.presentMode = info.presentModes.front();
	if (std::find(info.presentModes.begin(), info.presentModes.end(), gl::PresentMode::Immediate) != info.presentModes.end()) {
		ret.presentModeFast = gl::PresentMode::Immediate;
	}

	ret.presentMode = gl::PresentMode::Fifo;

	ret.imageFormat = info.formats.front().first;
	ret.colorSpace = info.formats.front().second;

	ret.transfer = (info.supportedUsageFlags & gl::ImageUsage::TransferDst) != gl::ImageUsage::None;

	return ret;
}

void Director::update() {
	auto t = _application->getClock();
	auto &size = _view->getScreenExtent();

	if (_time.global) {
		_time.delta = t - _time.global;
	} else {
		_time.delta = 0;
	}

	_time.global = t;
	_time.app = t - _startTime;

    // If we are debugging our code, prevent big delta time
    if (_time.delta && _time.delta > config::MaxDirectorDeltaTime) {
    	_time.delta = config::MaxDirectorDeltaTime;
    }

	if (_nextScene) {
		if (_scene) {
			_scene->onFinished(this);
		}
		_scene = _nextScene;

		auto d = _view->getDensity();

		_scene->setContentSize(Size2(size) / d);
		_scene->onPresented(this);
		_nextScene = nullptr;
	}

	// log::vtext("Director", "FrameOffset: ", platform::device::_clock() - _view->getFrameTime());

	_view->runFrame(_scene->getRenderQueue(), size);
	_inputDispatcher->update(_time);
	_scheduler->update(_time);

	_application->update(_time.delta);
}

void Director::begin(gl::View *view) {
	_view = view;

	if (_sizeChangedEvent) {
		removeHandlerNode(_sizeChangedEvent);
		_sizeChangedEvent = nullptr;
	}

	_sizeChangedEvent = onEventWithObject(gl::View::onScreenSize, view, [&] (const Event &) {
		if (_scene) {
			auto &size = _view->getScreenExtent();
			auto d = _view->getDensity();

			_scene->setContentSize(Size2(size) / d);

			updateGeneralTransform();
		}
	});

	updateGeneralTransform();

	if (_nextScene) {
		auto &size = _view->getScreenExtent();
		auto d = _view->getDensity();

		_scene = move(_nextScene);
		_scene->setContentSize(Size2(size) / d);
		_scene->onPresented(this);
		_nextScene = nullptr;
	}
}

void Director::end() {
	if (_scene) {
		_scene->onFinished(this);
		_scene = nullptr;
		_nextScene = nullptr;
	}
	_view = nullptr;
}

Size2 Director::getScreenSize() const {
	return _view->getScreenExtent();
}

void Director::runScene(Rc<Scene> &&scene) {
	if (!scene) {
		return;
	}

	/*auto linkId = retain();
	auto &queue = scene->getRenderQueue();
	_view->getLoop()->compileRenderQueue(queue, [this, scene, linkId] (bool success) {
		if (success) {
			auto linkId = retain();
			auto &queue = scene->getRenderQueue();
			_view->getLoop()->scheduleSwapchainRenderQueue(queue, [this, scene, linkId] {
				_nextScene = scene;
				release(linkId);
			});
		}
		release(linkId);
	});*/
}

float Director::getFps() const {
	return 1.0f / (_view->getLastFrameInterval() / 1000000.0f);
}

float Director::getAvgFps() const {
	return 1.0f / (_view->getAvgFrameInterval() / 1000000.0f);
}

float Director::getSpf() const {
	return _view->getLastFrameInterval() / 1000.0f;
}

float Director::getLocalFrameTime() const {
	return _view->getLastFrameTime() / 1000.0f;
}

void Director::invalidate() {

}

static inline int32_t sp_gcd (int16_t a, int16_t b) {
	int32_t c;
	while ( a != 0 ) {
		c = a; a = b%a;  b = c;
	}
	return b;
}

void Director::updateGeneralTransform() {
	auto d = _view->getDensity();
	auto size = Size2(_view->getScreenExtent()) / d;

	Mat4 proj;
	proj.scale(2.0f / size.width, -2.0f / size.height, -1.0);
	proj.m[12] = -1.0;
	proj.m[13] = 1.0;
	proj.m[14] = 0.0f;
	proj.m[15] = 1.0f;

	_generalProjection = proj;
}

}
