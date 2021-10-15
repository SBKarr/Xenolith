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

namespace stappler::xenolith {

XL_DECLARE_EVENT_CLASS(Director, onProjectionChanged);
XL_DECLARE_EVENT_CLASS(Director, onAfterUpdate);
XL_DECLARE_EVENT_CLASS(Director, onAfterVisit);
XL_DECLARE_EVENT_CLASS(Director, onAfterDraw);

Director::Director() { }

Director::~Director() { }

bool Director::init(Application *app, Rc<Scene> &&scene) {
	_application = app;
	_nextScene = move(scene);
	return true;
}

void Director::update() {
	uint64_t dt = 0;
	auto t = _application->getClock();

	if (_lastTime) {
		dt = t - _lastTime;
	}

	if (_nextScene) {
		if (_scene) {
			_scene->onExit();
		}
		_scene = _nextScene;

		auto &size = _view->getScreenSize();
		auto d = _view->getDensity();

		_scene->setContentSize(size / d);
		_scene->onEnter();
		_nextScene = nullptr;
	}

	_lastTime = t;
}


void Director::acquireInput(gl::FrameHandle &frame, const Rc<gl::AttachmentHandle> &a) {
	VertexArray array;
	array.init(4, 6);

	auto quad = array.addQuad();
	quad.setGeometry(Vec4(-0.5f, -0.5f, 0, 0), Size(1.0f, 1.0f));
	quad.setColor({
		Color::Red_500,
		Color::Green_500,
		Color::Blue_500,
		Color::White
	});

	frame.submitInput(a, Rc<gl::VertexData>(array.pop()));
}

Rc<gl::DrawScheme> Director::construct() {
	if (!_scene) {
		return nullptr;
	}

	auto df = gl::DrawScheme::create();

	auto p = df->getPool();
	memory::pool::push(p);

	do {
		RenderFrameInfo info;
		info.director = this;
		info.scene = _scene;
		info.pool = df->getPool();
		info.scheme = df.get();
		info.transformStack.reserve(8);
		info.zPath.reserve(8);

		info.transformStack.push_back(_generalProjection);

		_scene->render(info);
	} while (0);

	memory::pool::pop();

	return df;
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
		_scene = move(_nextScene);
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
	// General (2D) transform
	int32_t gcd = sp_gcd(size.width, size.height);
	int32_t dw = (int32_t)size.width / gcd;
	int32_t dh = (int32_t)size.height / gcd;
	int32_t dwh = gcd * dw * dh;

	float mod = 1.0f;
	while (dwh * mod > 16384) {
		mod /= 2.0f;
	}

	Mat4 proj;
	proj.scale(dh * mod, dw * mod, -1.0);
	proj.m[12] = -dwh * mod / 2.0f;
	proj.m[13] = -dwh * mod / 2.0f;
	proj.m[14] = dwh * mod / 2.0f - 1;
	proj.m[15] = dwh * mod / 2.0f + 1;
	proj.m[11] = -1.0f;

	_generalProjection = proj;
}

}
