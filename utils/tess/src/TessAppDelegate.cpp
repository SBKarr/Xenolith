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

#include "TessAppDelegate.h"
#include "TessScene.h"

#include "XLDirector.h"
#include "XLPlatform.h"
#include "XLFontLibrary.h"
#include "XLLabelParameters.h"
#include "XLResourceFontSource.h"
#include "XLVkRenderFontQueue.h"

namespace stappler::xenolith::tessapp {

static AppDelegate s_delegate;

AppDelegate::AppDelegate() { }

AppDelegate::~AppDelegate() { }

bool AppDelegate::onFinishLaunching() {
	if (!Application::onFinishLaunching()) {
		return false;
	}

	return true;
}

static Bytes openResourceFont(resources::fonts::FontName name) {
	auto d = resources::fonts::getFont(name);
	return data::decompress<memory::StandartInterface>(d.data(), d.size());
}

static String getResourceFontName(resources::fonts::FontName name) {
	return toString("resource:", resources::fonts::getFontName(name));
}

static font::FontController::FontQuery makeResourceFontQuery(resources::fonts::FontName name) {
	return font::FontController::FontQuery(
		getResourceFontName(name),
		[name] { return openResourceFont(name); }
	);
}

bool AppDelegate::onMainLoop() {
	_fontLibrary = Rc<font::FontLibrary>::create(_glLoop, Rc<vk::RenderFontQueue>::create("FontQueue"));

	using FontQuery = font::FontController::FontQuery;
	using FamilyQuery = font::FontController::FamilyQuery;
	using FontName = resources::fonts::FontName;

	_fontMainController = _fontLibrary->acquireController("AppFont", font::FontController::Query{
		Vector<FontQuery>{
			makeResourceFontQuery(FontName::OpenSans_Bold),
			makeResourceFontQuery(FontName::OpenSans_BoldItalic),
			makeResourceFontQuery(FontName::OpenSans_ExtraBold),
			makeResourceFontQuery(FontName::OpenSans_ExtraBoldItalic),
			makeResourceFontQuery(FontName::OpenSans_Italic),
			makeResourceFontQuery(FontName::OpenSans_Light),
			makeResourceFontQuery(FontName::OpenSans_LightItalic),
			makeResourceFontQuery(FontName::OpenSans_Medium),
			makeResourceFontQuery(FontName::OpenSans_MediumItalic),
			makeResourceFontQuery(FontName::OpenSans_Regular),
			makeResourceFontQuery(FontName::OpenSans_SemiBold),
			makeResourceFontQuery(FontName::OpenSans_SemiBoldItalic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_Bold),
			makeResourceFontQuery(FontName::OpenSans_Condensed_BoldItalic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_ExtraBold),
			makeResourceFontQuery(FontName::OpenSans_Condensed_ExtraBoldItalic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_Italic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_Light),
			makeResourceFontQuery(FontName::OpenSans_Condensed_LightItalic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_Medium),
			makeResourceFontQuery(FontName::OpenSans_Condensed_MediumItalic),
			makeResourceFontQuery(FontName::OpenSans_Condensed_Regular),
			makeResourceFontQuery(FontName::OpenSans_Condensed_SemiBold),
			makeResourceFontQuery(FontName::OpenSans_Condensed_SemiBoldItalic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_Bold),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_BoldItalic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_ExtraBold),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_ExtraBoldItalic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_Italic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_Light),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_LightItalic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_Medium),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_MediumItalic),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_Regular),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_SemiBold),
			makeResourceFontQuery(FontName::OpenSans_SemiCondensed_SemiBoldItalic),
			makeResourceFontQuery(FontName::Roboto_Black),
			makeResourceFontQuery(FontName::Roboto_BlackItalic),
			makeResourceFontQuery(FontName::Roboto_Bold),
			makeResourceFontQuery(FontName::Roboto_BoldItalic),
			makeResourceFontQuery(FontName::Roboto_Italic),
			makeResourceFontQuery(FontName::Roboto_Light),
			makeResourceFontQuery(FontName::Roboto_LightItalic),
			makeResourceFontQuery(FontName::Roboto_Medium),
			makeResourceFontQuery(FontName::Roboto_MediumItalic),
			makeResourceFontQuery(FontName::Roboto_Regular),
			makeResourceFontQuery(FontName::Roboto_Thin),
			makeResourceFontQuery(FontName::Roboto_ThinItalic),
			makeResourceFontQuery(FontName::RobotoCondensed_Bold),
			makeResourceFontQuery(FontName::RobotoCondensed_BoldItalic),
			makeResourceFontQuery(FontName::RobotoCondensed_Italic),
			makeResourceFontQuery(FontName::RobotoCondensed_Light),
			makeResourceFontQuery(FontName::RobotoCondensed_LightItalic),
			makeResourceFontQuery(FontName::RobotoCondensed_Regular),
			makeResourceFontQuery(FontName::RobotoMono_Bold),
			makeResourceFontQuery(FontName::RobotoMono_BoldItalic),
			makeResourceFontQuery(FontName::RobotoMono_ExtraLight),
			makeResourceFontQuery(FontName::RobotoMono_ExtraLightItalic),
			makeResourceFontQuery(FontName::RobotoMono_Italic),
			makeResourceFontQuery(FontName::RobotoMono_Light),
			makeResourceFontQuery(FontName::RobotoMono_LightItalic),
			makeResourceFontQuery(FontName::RobotoMono_Medium),
			makeResourceFontQuery(FontName::RobotoMono_MediumItalic),
			makeResourceFontQuery(FontName::RobotoMono_Regular),
			makeResourceFontQuery(FontName::RobotoMono_SemiBold),
			makeResourceFontQuery(FontName::RobotoMono_SemiBoldItalic),
			makeResourceFontQuery(FontName::RobotoMono_Thin),
			makeResourceFontQuery(FontName::RobotoMono_ThinItalic)
		},
		Vector<FamilyQuery>{
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Bold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_BoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::ExtraBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_ExtraBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::ExtraBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_ExtraBoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Italic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Light) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_LightItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Medium) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_MediumItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Regular) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::SemiBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::SemiBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiBoldItalic) }},

			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_Bold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_BoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::ExtraBold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_ExtraBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::ExtraBold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_ExtraBoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_Italic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_Light) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_LightItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Medium, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_Medium) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Medium, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_MediumItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_Regular) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::SemiBold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_SemiBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::SemiBold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_Condensed_SemiBoldItalic) }},

			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_Bold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_BoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::ExtraBold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_ExtraBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::ExtraBold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_ExtraBoldItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_Italic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_Light) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_LightItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Medium, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_Medium) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::Medium, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_MediumItalic) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_Regular) }},
			FamilyQuery{"OpenSans", font::FontStyle::Normal, font::FontWeight::SemiBold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_SemiBold) }},
			FamilyQuery{"OpenSans", font::FontStyle::Italic, font::FontWeight::SemiBold, font::FontStretch::SemiCondensed,
				Vector<String>{ getResourceFontName(FontName::OpenSans_SemiCondensed_SemiBoldItalic) }},

			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Black, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Black) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Black, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_BlackItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Bold) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_BoldItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Italic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Light) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_LightItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Medium) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_MediumItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Regular) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Thin, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_Thin) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Thin, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::Roboto_ThinItalic) }},

			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_Bold) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_BoldItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_Italic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_Light) }},
			FamilyQuery{"Roboto", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_LightItalic) }},
			FamilyQuery{"Roboto", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Condensed,
				Vector<String>{ getResourceFontName(FontName::RobotoCondensed_Regular) }},

			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Bold) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_BoldItalic) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::ExtraLight, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_ExtraLight) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::ExtraLight, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_ExtraLightItalic) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Italic) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Medium) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::Medium, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_MediumItalic) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Light) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::Light, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_LightItalic) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Regular) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::SemiBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_SemiBold) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::SemiBold, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_SemiBoldItalic) }},
			FamilyQuery{"monospace", font::FontStyle::Normal, font::FontWeight::Thin, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_Thin) }},
			FamilyQuery{"monospace", font::FontStyle::Italic, font::FontWeight::Thin, font::FontStretch::Normal,
				Vector<String>{ getResourceFontName(FontName::RobotoMono_ThinItalic) }}
		}
	});

	addView(gl::ViewInfo{
		"View-test",
		URect{0, 0, 1024, 768},
		0,
		[this] (const gl::SurfaceInfo &info) -> gl::SwapchainConfig {
			return selectConfig(info);
		},
		[this] (const Rc<Director> &dir) {
			onViewCreated(dir);
		},
		[this] () {
			end();
		}
	});

	wait(TimeInterval::milliseconds(100));

	_fontMainController = nullptr;
	_fontLibrary = nullptr;

	return true;
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

gl::SwapchainConfig AppDelegate::selectConfig(const gl::SurfaceInfo &info) {
	gl::SwapchainConfig ret;
	ret.extent = info.currentExtent;
	ret.imageCount = std::max(uint32_t(2), info.minImageCount);
	ret.presentMode = info.presentModes.front();

	if (std::find(info.presentModes.begin(), info.presentModes.end(), gl::PresentMode::Immediate) != info.presentModes.end()) {
		ret.presentModeFast = gl::PresentMode::Immediate;
	}

	ret.imageFormat = info.formats.front().first;
	ret.colorSpace = info.formats.front().second;
	ret.transfer = (info.supportedUsageFlags & gl::ImageUsage::TransferDst) != gl::ImageUsage::None;

	if (ret.presentMode == gl::PresentMode::Mailbox) {
		ret.imageCount = std::max(uint32_t(3), ret.imageCount);
	}

	return ret;
}

void AppDelegate::onViewCreated(const Rc<Director> &dir) {
	auto scene = Rc<TessScene>::create(this, dir->getScreenExtent());
	dir->runScene(move(scene));
}

}
