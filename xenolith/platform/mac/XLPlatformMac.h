/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_PLATFORM_MAC_XLPLATFORMMAC_H_
#define XENOLITH_PLATFORM_MAC_XLPLATFORMMAC_H_

#include "XLPlatform.h"
#include "XLVkView.h"

#if MACOS

namespace stappler::xenolith::platform::graphic {

class ViewImpl;

void ViewImpl_run(const Rc<ViewImpl> &);
float ViewImpl_getScreenDensity();
float ViewImpl_getSurfaceDensity(void *);
void *ViewImpl_getLayer(void *);
void ViewImpl_wakeup(const Rc<ViewImpl> &view);
void ViewImpl_setVSyncEnabled(void *, bool);

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&, float layerDensity);

	virtual void run() override;

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;
	virtual void update(bool displayLink) override;

	virtual void wakeup() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideString str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	virtual void mapWindow() override;

	void setOsView(void *v) { _osView = v; }
	void *getOsView() { return _osView; }

	StringView getTitle() const { return _name; }
	URect getFrame() const { return _rect; }

	void handleDisplayLinkCallback();
	void startLiveResize();
	void stopLiveResize();

protected:
	using vk::View::init;

	virtual bool pollInput(bool frameReady) override;

	virtual bool createSwapchain(gl::SwapchainConfig &&cfg, gl::PresentMode presentMode) override;

	virtual gl::SurfaceInfo getSurfaceOptions() const override;

	virtual void finalize() override;

	void *_osView = nullptr;
	URect _rect;
	String _name;
	bool _inputEnabled = false;
	bool _followDisplayLink = true;
	std::atomic_flag _displayLinkFlag;
};

}

#endif

#endif /* XENOLITH_PLATFORM_MAC_XLPLATFORMMAC_H_ */
