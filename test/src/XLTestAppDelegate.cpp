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

#include "XLDefine.h"
#include "XLDirector.h"
#include "XLTestAppDelegate.h"

#include "AppScene.h"
#include "XLPlatform.h"
#include "XLAppShaders.h"
#include "XLDefaultShaders.h"
#include "XLFontSourceSystem.h"
#include "XLFontFace.h"

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
	auto scene = Rc<AppScene>::create(Extent2(1024, 768));

	_glLoop->compileRenderQueue(scene->getRenderQueue(), [&] (bool success) {
		performOnMainThread([&] {
			runMainView(move(scene));
		});
		log::text("App", "Compiled");
	});

	auto ret = _loop->run();

	_fontMainController = nullptr;
	_fontLibrary = nullptr;

	return ret;
}

void AppDelegate::update(uint64_t dt) {
	Application::update(dt);
	if (_fontLibrary) {
		_fontLibrary->update();
	}
}

void AppDelegate::runMainView(Rc<AppScene> &&scene) {
	_fontLibrary = Rc<font::FontLibrary>::create(_glLoop, Rc<vk::RenderFontQueue>::create("FontQueue"));
	_fontLibrary->acquireController("AppFont", [this, scene] (Rc<font::FontController> &&c) {
		_fontMainController = move(c);
		log::text("App", "AppFont created");
		runFontTest();
		scene->addFontController(_fontMainController);
	});

	auto dir = Rc<Director>::create(this, move(scene));

	auto view = platform::graphic::createView(_loop, _glLoop, "Xenolith",
			URect{0, 0, uint32_t(_data.screenSize.width), uint32_t(_data.screenSize.height)});

	view->begin(dir, [this] {
		_loop->pushEvent(AppEvent::Terminate);
	});
}

void AppDelegate::runFontTest() {
	auto query = Rc<gl::RenderFontInput>::alloc();

	Vector<char16_t> chars;
	chars::CharGroup<char16_t, CharGroupId::Numbers>::foreach([&] (char16_t c) {
		chars.emplace_back(c);
	});
	chars::CharGroup<char16_t, CharGroupId::Cyrillic>::foreach([&] (char16_t c) {
		chars.emplace_back(c);
	});

	auto monoFace = _fontLibrary->openFontFace(font::SystemFontName::DejaVuSansMono, font::FontSize(24));
	auto regularFace = _fontLibrary->openFontFace(font::SystemFontName::DejaVuSans, font::FontSize(24));

	_fontMainController->updateTexture({
		pair(monoFace, chars),
		pair(regularFace, chars)
	});
}

}
