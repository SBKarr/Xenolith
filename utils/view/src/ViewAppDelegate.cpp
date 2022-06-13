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

#include "ViewAppDelegate.h"
#include "ViewScene.h"

#include "XLDirector.h"
#include "XLPlatform.h"
#include "XLFontLibrary.h"
#include "XLLabelParameters.h"

namespace stappler::xenolith::viewapp {

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
	addView(gl::ViewInfo{
		"View-test",
		URect{0, 0, 1024, 768},
		0,
		[] (const gl::SurfaceInfo &info) -> gl::SwapchainConfig {
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
		},
		[this] (const Rc<Director> &dir) {
			onViewCreated(dir);
		},
		[this] () {
			end();
		}
	});

	wait(TimeInterval::milliseconds(100));
	return true;
}

void AppDelegate::update(uint64_t dt) {
	Application::update(dt);
}

void AppDelegate::onViewCreated(const Rc<Director> &dir) {
	auto scene = Rc<ViewScene>::create(this, dir->getScreenExtent());
	dir->runScene(move(scene));
}

}
