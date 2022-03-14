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
#include "XLFontLibrary.h"
#include "XLLabelParameters.h"

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
	_fontLibrary = Rc<font::FontLibrary>::create(_glLoop, Rc<vk::RenderFontQueue>::create("FontQueue"));

	using FontQuery = font::FontController::FontQuery;
	using FamilyQuery = font::FontController::FamilyQuery;
	using SystemFontName = font::SystemFontName;

	_fontMainController = _fontLibrary->acquireController("AppFont", font::FontController::Query{
		Vector<FontQuery>{
			SystemFontName::DejaVuSans,
			SystemFontName::DejaVuSans_Bold,
			SystemFontName::DejaVuSans_BoldOblique,
			SystemFontName::DejaVuSans_ExtraLight,
			SystemFontName::DejaVuSans_Oblique,
			SystemFontName::DejaVuSansCondensed,
			SystemFontName::DejaVuSansCondensed_Bold,
			SystemFontName::DejaVuSansCondensed_BoldOblique,
			SystemFontName::DejaVuSansCondensed_Oblique,
			SystemFontName::DejaVuSansMono,
			SystemFontName::DejaVuSansMono_Bold,
			SystemFontName::DejaVuSansMono_BoldOblique,
			SystemFontName::DejaVuSansMono_Oblique,
		},
		Vector<FamilyQuery>{
			FamilyQuery{"DejaVuSans", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSans).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSans_Bold).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Oblique, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSans_BoldOblique).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Normal, font::FontWeight::W200, font::FontStretch::Normal,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSans_ExtraLight).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Oblique, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSans_Oblique).str() }},

			FamilyQuery{"DejaVuSans", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansCondensed).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansCondensed_Bold).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Oblique, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansCondensed_BoldOblique).str() }},
			FamilyQuery{"DejaVuSans", font::FontStyle::Oblique, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansCondensed_Oblique).str() }},

			FamilyQuery{"DejaVuSansMono", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansMono).str() }},
			FamilyQuery{"DejaVuSansMono", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansMono_Bold).str() }},
			FamilyQuery{"DejaVuSansMono", font::FontStyle::Oblique, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansMono_BoldOblique).str() }},
			FamilyQuery{"DejaVuSansMono", font::FontStyle::Oblique, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ font::getSystemFontName(SystemFontName::DejaVuSansMono_Oblique).str() }}
		}
	});

	auto scene = Rc<AppScene>::create(this, Extent2(1024, 768));

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
	if (_fontMainController) {
		_fontMainController->update();
	}
	if (_fontLibrary) {
		_fontLibrary->update();
	}
}

void AppDelegate::runMainView(Rc<AppScene> &&scene) {
	auto dir = Rc<Director>::create(this, move(scene));

	auto view = platform::graphic::createView(_loop, _glLoop, "Xenolith",
			URect{0, 0, uint32_t(_data.screenSize.width), uint32_t(_data.screenSize.height)});

	view->begin(dir, [this] {
		_loop->pushEvent(AppEvent::Terminate);
	});
}

}
