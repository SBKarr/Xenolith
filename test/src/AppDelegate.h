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

#ifndef TEST_XENOLITH_SRC_XLTESTAPPDELEGATE_H_
#define TEST_XENOLITH_SRC_XLTESTAPPDELEGATE_H_

#include "XLApplication.h"

namespace stappler::xenolith::app {

class AppScene;

class AppDelegate : public Application {
public:
	static EventHeader onSwapchainConfig;

    virtual ~AppDelegate();

    virtual bool onFinishLaunching() override;
	virtual bool onMainLoop() override;

	gl::SurfaceInfo getSurfaceinfo() const { return _surfaceInfo; }
	gl::SwapchainConfig getSwapchainConfig() const { return _swapchainConfig; }

	void setPreferredPresentMode(gl::PresentMode);

	Rc<renderqueue::Queue> getShadowQueue() const { return _shadowQueueLoaded ? _shadowQueue : nullptr; }

protected:
	gl::SwapchainConfig selectConfig(const gl::SurfaceInfo &info);
	void onViewCreated(const Rc<Director> &);

	Mutex _configMutex;
	gl::PresentMode _preferredPresentMode = gl::PresentMode::Unsupported;

	gl::SurfaceInfo _surfaceInfo;
	gl::SwapchainConfig _swapchainConfig;

	bool _shadowQueueLoaded = false;
	Rc<renderqueue::Queue> _shadowQueue;
};

}

#endif /* TEST_XENOLITH_SRC_XLTESTAPPDELEGATE_H_ */
