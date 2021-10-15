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

#include "XLDefine.h"
#include "XLDirector.h"
#include "XLTestAppDelegate.h"
#include "XLPlatform.h"
#include "XLAppScene.h"
#include "XLAppShaders.h"
#include "XLDefaultShaders.h"

namespace stappler::xenolith::app {

static AppDelegate s_delegate;

AppDelegate::AppDelegate() { }

AppDelegate::~AppDelegate() { }

bool AppDelegate::onFinishLaunching() {
	if (!Application::onFinishLaunching()) {
		return false;
	}

	return true;
}

bool AppDelegate::onMainLoop() {
	auto device = getGlInstance()->makeDevice();
	_glLoop = Rc<gl::Loop>::alloc(this, device);
	_glLoop->begin();

	auto scene = Rc<AppScene>::create(Extent2(1024, 768));

	_glLoop->compileRenderQueue(scene->getRenderQueue(), [&] (bool success) {
		performOnMainThread([&] {
			runMainView(move(scene));
		});
		log::text("App", "Compiled");
	});

	auto ret = _loop->run();
	_glLoop->end();
	_glLoop = nullptr;
	return ret;
}

void AppDelegate::runMainView(Rc<Scene> &&scene) {
	auto dir = Rc<Director>::create(this, move(scene));

	auto view = platform::graphic::createView(_loop, _glLoop, "Xenolith",
			URect{0, 0, uint32_t(_data.screenSize.width), uint32_t(_data.screenSize.height)});

	view->begin(dir, [this] {
		_loop->pushEvent(AppEvent::Terminate);
	});
}

}
