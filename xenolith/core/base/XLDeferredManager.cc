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

#include "XLDeferredManager.h"
#include "XLApplication.h"
#include "XLVectorCanvas.h"

namespace stappler::xenolith {

DeferredManager::~DeferredManager() {

}

DeferredManager::DeferredManager(Application *app, StringView name)
: thread::TaskQueue(name) {
	_application = app;
}

bool DeferredManager::init(uint32_t threadCount) {
	if (!spawnWorkers(thread::TaskQueue::Flags::None, Application::DeferredThreadId, threadCount, getName())) {
		log::text("Application", "Fail to spawn worker threads");
		return false;
	}

	return true;
}

void DeferredManager::cancel() {
	cancelWorkers();
}

void DeferredManager::update() {
	thread::TaskQueue::update();
}

Rc<VectorCanvasDeferredResult> DeferredManager::runVectorCavas(Rc<VectorImageData> &&image, Size2 targetSize, Color4F color, float quality) {
	auto result = new std::promise<Rc<VectorCanvasResult>>;
	Rc<VectorCanvasDeferredResult> ret = Rc<VectorCanvasDeferredResult>::create(result->get_future());
	perform([this, image = move(image), targetSize, color, quality, ret, result] () mutable {
		auto canvas = VectorCanvas::getInstance();
		canvas->setColor(color);
		canvas->setQuality(quality);
		auto res = canvas->draw(move(image), targetSize);
		result->set_value(res);

		_application->performOnMainThread([ret = move(ret), res = move(res), result] () mutable {
			ret->handleReady(move(res));
			delete result;
		});
	}, ret);
	return ret;
}

}
