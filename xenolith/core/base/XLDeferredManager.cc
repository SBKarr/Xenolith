/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLDeferredManager.h"
#include "XLApplication.h"
#include "XLVectorCanvas.h"
#include "XLFontLibrary.h"

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

Rc<VectorCanvasDeferredResult> DeferredManager::runVectorCavas(Rc<VectorImageData> &&image, Size2 targetSize, Color4F color, float quality, bool waitOnReady) {
	auto result = new std::promise<Rc<VectorCanvasResult>>;
	Rc<VectorCanvasDeferredResult> ret = Rc<VectorCanvasDeferredResult>::create(result->get_future(), waitOnReady);
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

Rc<LabelDeferredResult> DeferredManager::runLabel(Label::FormatSpec *format, const Color4F &color) {
	auto result = new std::promise<Rc<LabelResult>>;
	Rc<LabelDeferredResult> ret = Rc<LabelDeferredResult>::create(result->get_future());
	perform([this, format = Rc<Label::FormatSpec>(format), color, ret, result] () mutable {
		auto res = Label::writeResult(format, color);
		result->set_value(res);

		_application->performOnMainThread([ret = move(ret), res = move(res), result] () mutable {
			ret->handleReady(move(res));
			delete result;
		});
	}, ret);
	return ret;
}

struct DeferredFontRequestsData : Ref {
	virtual ~DeferredFontRequestsData() { }

	DeferredFontRequestsData(const Rc<font::FontLibrary> &lib, const Vector<font::FontUpdateRequest> &req)
	: library(lib) {
		for (auto &it : req) {
			nrequests += it.chars.size();
		}

		fontRequests.reserve(nrequests);

		for (uint32_t i = 0; i < req.size(); ++ i) {
			faces.emplace_back(req[i].object);
			for (auto &it : req[i].chars) {
				fontRequests.emplace_back(i, it);
			}
		}
	}

	void runThread() {
		Vector<Rc<font::FontFaceObjectHandle>> threadFaces; threadFaces.resize(faces.size(), nullptr);
		uint32_t target = current.fetch_add(1);
		uint32_t c = 0;
		while (target < nrequests) {
			auto &v = fontRequests[target];
			if (v.second == 0) {
				c = complete.fetch_add(1);
				target = current.fetch_add(1);
				continue;
			}

			if (!threadFaces[v.first]) {
				threadFaces[v.first] = library->makeThreadHandle(faces[v.first]);
			}

			threadFaces[v.first]->acquireTexture(v.second, [&] (const font::CharTexture &tex) {
				onTexture(v.first, tex);
			});
			c = complete.fetch_add(1);
			target = current.fetch_add(1);
		}
		threadFaces.clear();
		if (c == nrequests - 1) {
			onComplete();
		}
	}

	std::atomic<uint32_t> current = 0;
	std::atomic<uint32_t> complete = 0;
	uint32_t nrequests = 0;
	Vector<Rc<font::FontFaceObject>> faces;
	Vector<Pair<uint32_t, char16_t>> fontRequests;

	Rc<font::FontLibrary> library;
	Function<void(uint32_t reqIdx, const font::CharTexture &texData)> onTexture;
	Function<void()> onComplete;
};

void DeferredManager::runFontRenderer(const Rc<font::FontLibrary> &lib,
		const Vector<font::FontUpdateRequest> &req,
		Function<void(uint32_t reqIdx, const font::CharTexture &texData)> &&onTexture,
		Function<void()> &&onComplete) {

	auto data = Rc<DeferredFontRequestsData>::alloc(lib, req);
	data->onTexture = move(onTexture);
	data->onComplete = move(onComplete);

	for (uint32_t i = 0; i < getThreadCount(); ++ i) {
		perform([data] () {
			data->runThread();
		});
	}
}

}
