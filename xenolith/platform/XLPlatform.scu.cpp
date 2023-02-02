/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLDefine.h"

#include "linux/XLVkViewImpl.cc"
#include "linux/XLVkViewWayland.cc"
#include "linux/XLVkViewXcb.cc"
#include "linux/XLVulkan.cc"
#include "linux/XLDevice.cc"
#include "linux/XLInteraction.cc"
#include "linux/XLLinuxDBus.cc"
#include "linux/XLLinuxWayland.cc"
#include "linux/XLLinuxXcb.cc"
#include "linux/XLLinuxXkb.cc"
#include "linux/XLNetwork.cc"

#include "mac/XLDevice.cc"
#include "mac/XLInteraction.cc"
#include "mac/XLNetwork.cc"
#include "mac/XLVulkan.cc"
#include "mac/XLVkViewImpl.cc"

#include "android/XLJni.cc"
#include "android/XLDevice.cc"
#include "android/XLInteraction.cc"
#include "android/XLNetwork.cc"
#include "android/XLVulkan.cc"
#include "android/XLNativeActivity.cc"
#include "android/XLVkViewImpl.cc"

/*#include "win32/XLDevice.cc"
#include "win32/XLInteraction.cc"
#include "win32/XLNetwork.cc"

#include "desktop/XLVkViewImpl-desktop.cc"*/
