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

#include "XLDefine.h"

#include "XLEvent.cc"
#include "XLEventHeader.cc"
#include "XLEventHandler.cc"

#include "XLDirector.cc"
#include "XLResourceCache.cc"

#include "XLVertexArray.cc"

#include "XLScene.cc"
#include "XLScheduler.cc"

//#include "XLStaticResource.cc"

namespace stappler::xenolith::profiling {

ProfileData begin(StringView tag, StringView variant, uint64_t limit) {
	ProfileData ret;
	ret.tag = tag;
	ret.variant = variant;
	ret.limit = limit;
	ret.timestamp = platform::device::_clock(platform::device::Thread);
	return ret;
}

void end(ProfileData &data) {
	if (!data.limit) {
		return;
	}

	auto dt = platform::device::_clock(platform::device::Thread) - data.timestamp;
	if (dt > data.limit) {
		log::vtext("Profiling", "[", data.tag , "] [", data.variant, "] limit exceeded: ", dt, " (from ", data.limit, ")");
		store(data);
	}
}

void store(ProfileData &data) {

}

}
