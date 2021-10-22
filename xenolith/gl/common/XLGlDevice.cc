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

#include "XLGlDevice.h"
#include "XLGlFrame.h"
#include "XLGlLoop.h"
#include "XLGlRenderQueue.h"
#include "XLGlObject.h"
#include "XLGlFrame.h"
#include "XLApplication.h"
#include "SPThreadTaskQueue.h"

namespace stappler::xenolith::gl {

Device::Device() { }

Device::~Device() {
	invalidateObjects();
}

bool Device::init(const Instance *instance) {
	_glInstance = instance;
	_samplersInfo.emplace_back(SamplerInfo());

	return true;
}

void Device::begin(const Application *, thread::TaskQueue &) {
	_started = true;
}

void Device::end(thread::TaskQueue &) {
	_started = false;
}

void Device::waitIdle() {

}

void Device::defineSamplers(Vector<SamplerInfo> &&info) {
	if (isSamplersCompiled()) {
		log::text("Gl-Device", "Fail to define sampler list - samplers already compiled");
	} else {
		_samplersInfo = move(info);
	}
}

void Device::invalidateFrame(FrameHandle &frame) { }

Rc<Shader> Device::getProgram(StringView name) {
	std::unique_lock<Mutex> lock(_mutex);
	auto it = _shaders.find(name);
	if (it != _shaders.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<Shader> Device::addProgram(Rc<Shader> program) {
	std::unique_lock<Mutex> lock(_mutex);
	auto it = _shaders.find(program->getName());
	if (it == _shaders.end()) {
		_shaders.emplace(program->getName().str(), program);
		return program;
	} else {
		return it->second;
	}
}

void Device::compileResource(thread::TaskQueue &q, const Rc<Resource> &req, Function<void(bool)> &&complete) {
	/**/
}

void Device::compileRenderQueue(gl::Loop &loop, const Rc<RenderQueue> &queue, Function<void(bool)> &&complete) {
	if (!_started && !isSamplersCompiled() && queue->usesSamplers()) {
		compileSamplers(*loop.getQueue());
	}
	/*if (queue) {
		auto p = Rc<CompilationProcess>::alloc(this, queue, [this, queue, complete = move(complete)] (bool success) {
			if (complete) {
				complete(success);
			}
		});
		q.perform(Rc<Task>::create([p, queue = Rc<thread::TaskQueue>(&q)] (const thread::Task &) -> bool {
			p->runShaders(*queue);
			return true;
		}));
		if (auto res = queue->getInternalResource()) {
			compileResource(q, res, [p] (bool success) {
				if (success) {
					if (p->pipelinesInQueue.fetch_sub(1) == 1) {
						p->complete();
					}
				} else {
					p->fail();
				}
			});
		}
	} else if (complete) {
		complete(false);
	}*/
}

void Device::compileMaterials(gl::Loop &loop, Rc<MaterialInputData> &&) {

}

void Device::addObject(ObjectInterface *obj) {
	_objects.emplace(obj);
}

void Device::removeObject(ObjectInterface *obj) {
	_objects.erase(obj);
}

void Device::onLoopStarted(gl::Loop &) {

}

void Device::onLoopEnded(gl::Loop &) {

}

void Device::clearShaders() {
	_shaders.clear();
}

void Device::invalidateObjects() {
	auto objs = std::move(_objects);
	_objects.clear();
	for (auto &it : objs) {
		log::vtext("Gl-Device", "Object ", typeid(*it).name(), " was not destroyed before device destruction");
		it->invalidate();
	}
}

}
