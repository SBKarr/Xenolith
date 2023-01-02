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

#ifndef XENOLITH_CORE_BASE_XLDEFERREDMANAGER_H_
#define XENOLITH_CORE_BASE_XLDEFERREDMANAGER_H_

#include "SPThreadTaskQueue.h"
#include "XLVectorResult.h"
#include "XLLabel.h"

namespace stappler::xenolith {

class DeferredManager : protected thread::TaskQueue {
public:
	virtual ~DeferredManager();

	DeferredManager(Application *, StringView);

	bool init(uint32_t threadCount);
	void cancel();

	void update();

	Rc<VectorCanvasDeferredResult> runVectorCavas(Rc<VectorImageData> &&image, Size2 targetSize, Color4F color, float quality);
	Rc<LabelDeferredResult> runLabel(Label::FormatSpec *format, const Color4F &);

	void runFontRenderer(const Rc<font::FontLibrary> &,
			const Vector<font::FontUpdateRequest> &req,
			Function<void(uint32_t reqIdx, const font::CharTexture &texData)> &&,
			Function<void()> &&);

	using mem_std::AllocBase::operator new;
	using mem_std::AllocBase::operator delete;

	using Ref::release;
	using Ref::retain;

protected:
	Application *_application = nullptr;
};

}

#endif /* XENOLITH_CORE_BASE_XLDEFERREDMANAGER_H_ */
