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

bool Device::init(const Rc<Instance> &instance) {
	_glInstance = instance;
	return true;
}

void Device::begin(Application *, thread::TaskQueue &) {

}

void Device::end(thread::TaskQueue &) {

}

void Device::reset(thread::TaskQueue &) {

}

void Device::waitIdle() {

}

Rc<gl::FrameHandle> Device::beginFrame(gl::Loop &loop, gl::RenderQueue &queue) {
	if (!canStartFrame()) {
		scheduleNextFrame();
		return nullptr;
	}

	_nextFrameScheduled = false;
	auto frame = makeFrame(loop, queue, _frames.empty());
	if (frame && frame->isValidFlag()) {
		_frames.push_back(frame);
		return frame;
	}
	return nullptr;
}

void Device::setFrameSubmitted(Rc<gl::FrameHandle> frame) {
	auto it = _frames.begin();
	while (it != _frames.end()) {
		if ((*it) == frame) {
			it = _frames.erase(it);
		} else {
			++ it;
		}
	}

	for (auto &it : _frames) {
		if (!it->isReadyForSubmit()) {
			it->setReadyForSubmit(true);
			break;
		}
	}

	if (_nextFrameScheduled) {
		frame->getLoop()->pushEvent(Loop::Event::FrameTimeoutPassed);
	}
}

void Device::invalidateFrame(gl::FrameHandle &handle) {
	auto it = std::find(_frames.begin(), _frames.end(), &handle);
	if (it != _frames.end()) {
		_frames.erase(it);
	}
}

bool Device::isFrameValid(const gl::FrameHandle &handle) {
	if (handle.getGen() == _gen && std::find(_frames.begin(), _frames.end(), &handle) != _frames.end()) {
		return true;
	}
	return false;
}

void Device::incrementGeneration() {
	++ _gen;
	if (!_frames.empty()) {
		auto f = move(_frames);
		_frames.clear();
		for (auto &it : f) {
			it->invalidate();
		}
	}
}

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

struct CompilationProcess : public Ref {
	virtual ~CompilationProcess() { }
	CompilationProcess(const Rc<Device> &dev, const Rc<RenderQueue> &req)
	 : draw(dev), req(req) { }

	void runShaders(thread::TaskQueue &);
	void runPipelines(thread::TaskQueue &);
	void complete();

	std::atomic<size_t> programsInQueue = 0;
	std::atomic<size_t> pipelinesInQueue = 0;

	Rc<Device> draw;
	Rc<RenderQueue> req;
	Vector<Rc<RenderPassImpl>> passes;
};

void CompilationProcess::runShaders(thread::TaskQueue &queue) {
	retain(); // release in complete;

	size_t tasksCount = 0;
	Vector<ProgramData *> programs;

	++ programsInQueue;

	programsInQueue += req->getPasses().size();
	tasksCount += req->getPasses().size();
	for (auto &it : req->getPrograms()) {
		if (auto p = draw->getProgram(it->key)) {
			it->program = p;
		} else {
			++ tasksCount;
			++ programsInQueue;
			programs.emplace_back(it);
		}
	}

	for (auto &it : programs) {
		queue.perform(Rc<Task>::create([this, req = it, queue = Rc<thread::TaskQueue>(&queue)] (const thread::Task &) -> bool {
			auto ret = draw->makeShader(*req);
			if (!ret) {
				log::vtext("vk", "Fail to compile shader program ", req->key);
				return false;
			} else {
				req->program = draw->addProgram(ret);
				if (programsInQueue.fetch_sub(1) == 1) {
					runPipelines(*queue);
				}
			}
			return true;
		}));
	}

	req->prepare();

	for (auto &it : req->getPasses()) {
		queue.perform(Rc<Task>::create([this, req = it, queue = Rc<thread::TaskQueue>(&queue)] (const thread::Task &) -> bool {
			auto ret = draw->makeRenderPass(*req);
			if (!ret) {
				log::vtext("vk", "Fail to compile render pass ", req->key);
				return false;
			} else {
				req->impl = ret;
				if (programsInQueue.fetch_sub(1) == 1) {
					runPipelines(*queue);
				}
			}
			return true;
		}));
	}

	queue.perform(Rc<Task>::create([this, queue = Rc<thread::TaskQueue>(&queue)] (const thread::Task &) -> bool {
		auto ret = draw->makePipelineLayout(req->getPipelineLayout());
		if (!ret) {
			log::vtext("vk", "Fail to compile pipeline layout ", req->getName());
			return false;
		} else {
			req->setPipelineLayout(move(ret));
			if (programsInQueue.fetch_sub(1) == 1) {
				runPipelines(*queue);
			}
		}
		return true;
	}));

	if (tasksCount == 0) {
		runPipelines(queue);
	}
}

void CompilationProcess::runPipelines(thread::TaskQueue &queue) {
	size_t tasksCount = 0;
	for (auto &pit : req->getPasses()) {
		pipelinesInQueue += pit->pipelines.size();
		tasksCount += pit->pipelines.size();
	}

	for (auto &pit : req->getPasses()) {
		for (auto &it : pit->pipelines) {
			queue.perform(Rc<Task>::create([this, pass = pit, pipeline = it] (const thread::Task &) -> bool {
				auto ret = draw->makePipeline(*req, *pass, *pipeline);
				if (!ret) {
					log::vtext("vk", "Fail to compile pipeline ", pipeline->key);
					return false;
				} else {
					pipeline->pipeline = ret;
					if (pipelinesInQueue.fetch_sub(1) == 1) {
						complete();
					}
				}
				return true;
			}));
		}
	}

	if (tasksCount == 0) {
		complete();
	}
}

void CompilationProcess::complete() {
	Application::getInstance()->performOnMainThread([req = req] {
		if (req) {
			req->setCompiled(true);
		}
	});
	release(); // release in complete;
}


void Device::compileResource(thread::TaskQueue &queue, const Rc<Resource> &req) {
	/**/
}

void Device::compileRenderQueue(thread::TaskQueue &queue, const Rc<RenderQueue> &req) {
	auto p = Rc<CompilationProcess>::alloc(this, req);

	queue.perform(Rc<Task>::create([p, queue = Rc<thread::TaskQueue>(&queue)] (const thread::Task &) -> bool {
		p->runShaders(*queue);
		return true;
	}));

}

Rc<RenderQueue> Device::createDefaultRenderQueue(const Rc<gl::Loop> &loop, thread::TaskQueue &queue, const gl::ImageInfo &info) {
	auto app = loop->getApplication();

	if (auto ret = app->onDefaultRenderQueue(info)) {
		for (auto &it : ret->getResources()) {
			compileResource(queue, it);
		}

		compileRenderQueue(queue, ret);
		queue.waitForAll();
		return ret;
	}

	return nullptr;
}

void Device::addObject(ObjectInterface *obj) {
	_objects.emplace(obj);
}

void Device::removeObject(ObjectInterface *obj) {
	_objects.erase(obj);
}

const Rc<gl::RenderQueue> Device::getDefaultRenderQueue() const {
	return nullptr;
}

bool Device::canStartFrame() const {
	if (_frames.size() >= 2) {
		return false;
	}

	for (auto &it : _frames) {
		if (!it->isSubmitted()) {
			return false;
		}
	}

	return true;
}

bool Device::scheduleNextFrame() {
	auto prev = _nextFrameScheduled;
	_nextFrameScheduled = true;
	return prev;
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
