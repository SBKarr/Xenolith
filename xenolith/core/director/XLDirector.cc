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
#include "XLGlLoop.h"
#include "XLScene.h"
#include "XLVertexArray.h"
#include "XLScheduler.h"
#include "XLActionManager.h"

namespace stappler::xenolith {

Director::Director() { }

Director::~Director() { }

bool Director::init(Application *app, gl::View *view) {
	_application = app;
	_view = view;
	_pool = Rc<PoolRef>::alloc();
	_pool->perform([&] {
		_scheduler = Rc<Scheduler>::create();
		_actionManager = Rc<ActionManager>::create();
		_inputDispatcher = Rc<InputDispatcher>::create(_pool, _view);
	});
	_startTime = _application->getClock();
	_time.global = 0;
	_time.app = 0;
	_time.delta = 0;

	_sizeChangedEvent = onEventWithObject(gl::View::onScreenSize, view, [&] (const Event &ev) {
		auto s = ev.getDataValue().getValue("size");
		_screenSize = _screenExtent = Extent2(s.getInteger(0), s.getInteger(1));
		_density = ev.getDataValue().getDouble("density");

		if (_scene) {
			_scene->setDensity(_density);
			_scene->setContentSize(_screenSize / _scene->getDensity());

			updateGeneralTransform();
		}
	});

	_screenExtent = _view->getScreenExtent();
	_screenSize = _view->getScreenExtent();
	_density = _view->getDensity();

	updateGeneralTransform();

	return true;
}

TextInputManager *Director::getTextInputManager() const {
	return _inputDispatcher->getTextInputManager();
}

const Rc<ResourceCache> &Director::getResourceCache() const {
	return _application->getResourceCache();
}

bool Director::acquireFrame(const Rc<FrameRequest> &req) {
	float density = 1.0f;
	if (_scene) {
		density = _scene->getDensity();
	}
	if (_screenExtent != req->getExtent() || density != req->getDensity()) {
		_screenSize = _screenExtent = req->getExtent();

		if (_scene) {
			_scene->setDensity(req->getDensity());
			_scene->setContentSize(_screenSize / _scene->getDensity());
		}
		updateGeneralTransform();
	}

	update();
	if (_scene) {
		_scene->specializeRequest(req);
	}

	_application->performOnMainThread([this, req] {
		_scene->renderRequest(req);
	}, this, true);

	return true;
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

    _time.dt = float(_time.delta) / 1'000'000;

	if (_nextScene) {
		if (_scene) {
			_scene->onFinished(this);
		}
		_scene = _nextScene;

		_scene->setContentSize(_screenSize / _scene->getDensity());
		_scene->onPresented(this);
		_nextScene = nullptr;
	}

	// log::vtext("Director", "FrameOffset: ", platform::device::_clock() - _view->getFrameTime());

	//_view->runFrame(_scene->getRenderQueue(), _screenSize);

	_inputDispatcher->update(_time);
	_scheduler->update(_time);
	_actionManager->update(_time);

	_application->getResourceCache()->update(this, _time);

	_autorelease.clear();
}

void Director::end() {
	if (_scene) {
		_scene->onFinished(this);
		_scene = nullptr;
		_nextScene = nullptr;
	}
	_view = nullptr;
}

void Director::runScene(Rc<Scene> &&scene) {
	if (!scene) {
		return;
	}

	auto linkId = retain();
	auto &queue = scene->getRenderQueue();
	_view->getLoop()->compileRenderQueue(queue, [this, scene = move(scene), linkId] (bool success) mutable {
		if (success) {
			_application->performOnMainThread([this, scene = move(scene)] {
				_nextScene = scene;
				if (!_scene) {
					_scene = _nextScene;
					_nextScene = nullptr;
					_scene->setContentSize(_screenSize / _scene->getDensity());
					updateGeneralTransform();
					_scene->onPresented(this);
					_view->getLoop()->performOnGlThread([view = _view, scene = _scene] {
						view->runWithQueue(scene->getRenderQueue());
					}, this);
				}
			}, this);
		}
		release(linkId);
	});
}

void Director::pushDrawStat(const gl::DrawStat &stat) {
	_drawStat = stat;
}

float Director::getFps() const {
	return 1.0f / (_view->getLastFrameInterval() / 1000000.0f);
}

float Director::getAvgFps() const {
	return 1.0f / (_view->getAvgFrameInterval() / 1000000.0f);
}

float Director::getSpf() const {
	return _view->getLastFrameTime() / 1000.0f;
}

float Director::getLocalFrameTime() const {
	return _view->getAvgFenceTime() / 1000.0f;
}

void Director::autorelease(Ref *ref) {
	_autorelease.emplace_back(ref);
}

void Director::invalidate() {

}

void Director::updateGeneralTransform() {
	if (_scene) {
		auto size = _screenSize /* / _scene->getDensity() */;

		Mat4 proj;
		proj.scale(2.0f / size.width, -2.0f / size.height, -1.0);
		proj.m[12] = -1.0;
		proj.m[13] = 1.0;
		proj.m[14] = 0.0f;
		proj.m[15] = 1.0f;

		_generalProjection = proj;
	}
}

}
