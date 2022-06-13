/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_COMMON_XLGLINSTANCE_H_
#define XENOLITH_GL_COMMON_XLGLINSTANCE_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

struct DeviceProperties {
	String deviceName;
	uint32_t apiVersion = 0;
	uint32_t driverVersion = 0;
	bool supportsPresentation = false;
};

class Instance : public Ref {
public:
	using TerminateCallback = Function<void()>;

	static constexpr uint32_t DefaultDevice = maxOf<uint32_t>();

	static String getVersionDescription(uint32_t);

	Instance(TerminateCallback &&);
	virtual ~Instance();

	const Vector<DeviceProperties> &getAvailableDevices() const { return _availableDevices; }

	virtual Rc<Loop> makeLoop(Application *, uint32_t deviceIndex) const;

protected:
	TerminateCallback _terminate;
	Vector<DeviceProperties> _availableDevices;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLINSTANCE_H_ */
