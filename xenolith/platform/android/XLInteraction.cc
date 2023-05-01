/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatform.h"

#if ANDROID

#include "XLPlatformAndroid.h"

namespace stappler::xenolith::platform::interaction {

bool _goToUrl(void *handle, StringView url, bool external) {
	((NativeActivity *)handle)->openUrl(url);
	return true;
}

void _makePhoneCall(void *handle, StringView str) {
	((NativeActivity *)handle)->openUrl(str);
}

void _mailTo(void *handle, StringView address) {
	((NativeActivity *)handle)->openUrl(address);
}

void _notification(void *handle, StringView title, StringView text) {
	//((NativeActivity *)handle)->notification(title, text);
}

void _openFileDialog(const String &path, const Function<void(const String &)> &func) {
	if (func) {
		func("");
	}
}

}

#endif
