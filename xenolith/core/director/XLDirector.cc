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

void Director::update() {
	auto t = _application->getClock();

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
			_scene->onExit();
		}
		_scene = _nextScene;

		auto &size = _view->getScreenSize();
		auto d = _view->getDensity();

		_scene->setContentSize(size / d);
		_scene->onPresented(this);
		_nextScene = nullptr;
	}

	_scheduler->update(_time);
}

void Director::begin(gl::View *view) {
	_view = view;

	if (_sizeChangedEvent) {
		removeHandlerNode(_sizeChangedEvent);
		_sizeChangedEvent = nullptr;
	}

	_sizeChangedEvent = onEventWithObject(gl::View::onScreenSize, view, [&] (const Event &) {
		if (_scene) {
			auto &size = _view->getScreenSize();
			auto d = _view->getDensity();

			_scene->setContentSize(size / d);

			updateGeneralTransform();
		}
	});

	updateGeneralTransform();

	if (_nextScene) {
		auto &size = _view->getScreenSize();
		auto d = _view->getDensity();

		_scene = move(_nextScene);
		_scene->setContentSize(size / d);
		_scene->onPresented(this);
		_nextScene = nullptr;
	}
}

void Director::end() {
	if (_scene) {
		_scene->onExit();
		_scene->onFinished(this);
		_scene = nullptr;
		_nextScene = nullptr;
	}
	_view = nullptr;
}

Size Director::getScreenSize() const {
	return _view->getScreenSize();
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
	auto size = _view->getScreenSize() / d;

	Mat4 proj;
	proj.scale(2.0f / size.width, -2.0f / size.height, -1.0);
	proj.m[12] = -1.0;
	proj.m[13] = 1.0;
	proj.m[14] = 0.0f;
	proj.m[15] = 1.0f;

	_generalProjection = proj;
}

}
