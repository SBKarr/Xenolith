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

#ifndef COMPONENTS_XENOLITH_PLATFORM_COMMON_XLPLATFORM_H_
#define COMPONENTS_XENOLITH_PLATFORM_COMMON_XLPLATFORM_H_

#include "XLDefine.h"

namespace stappler::xenolith {

class EventLoopInterface;
class Application;

}

namespace stappler::xenolith::gl {

class Instance;
class View;
class Loop;
class Device;
struct ViewInfo;

}

/* Platform-depended functions interface */
namespace stappler::xenolith::platform {

namespace version {
	inline const char *_name() { return "Stappler+Xenolith"; }
	inline uint32_t _version() { return XL_MAKE_API_VERSION(0, 1, 0, 0); }
}

namespace network {
	void _setNetworkCallback(const Function<void(bool isOnline)> &callback);
	bool _isNetworkOnline();
}

namespace device {
	enum ClockType {
		Default,
		Monotonic,
		Realtime,
		Process,
		Thread,
	};

	String _userAgent();
	String _deviceIdentifier();

	uint64_t _clock(ClockType = Default);
	void _sleep(uint64_t microseconds);
}

namespace interaction {
	bool _goToUrl(void *handle, StringView url, bool external);
	void _makePhoneCall(void *handle, StringView number);
	void _mailTo(void *handle, StringView address);
	void _notification(void *handle, StringView title, StringView text);
}

namespace statusbar {
	enum class _StatusBarColor : uint8_t {
        Light = 1,
        Black = 2,
	};
	void _setEnabled(bool enabled);
	bool _isEnabled();
	void _setColor(_StatusBarColor color);
	float _getHeight(const Size2 &screenSize, bool isTablet);
}

namespace graphic {
	Rc<gl::Instance> createInstance(Application *);

	Rc<gl::View> createView(gl::Loop &, gl::Device &, gl::ViewInfo &&);

	// should be commonly supported format,
	// R8G8B8A8_UNORM on Android, B8G8R8A8_UNORM on others
	gl::ImageFormat getCommonFormat();
}

}

#endif
