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

#ifndef XENOLITH_PLATFORM_COMMON_XLEVENTLOOP_H_
#define XENOLITH_PLATFORM_COMMON_XLEVENTLOOP_H_

#include "XLDefine.h"


namespace stappler::xenolith::gl {

class View;

}

namespace stappler::xenolith {

class EventLoopInterface : public Ref {
public:
	// false on timeout (or signal/error), true on wakeup
	virtual bool poll(uint64_t microsecondsTimeout) = 0;
	virtual void wakeup() = 0;

	virtual void addView(gl::View *) = 0;
	virtual void removeView(gl::View *) = 0;

	virtual void end() = 0;
};

}

#endif /* XENOLITH_PLATFORM_COMMON_XLEVENTLOOP_H_ */
