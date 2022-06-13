/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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
#include "XLGlLoop.h"
#include "XLGlObject.h"
#include "XLApplication.h"
#include "XLRenderQueueImageStorage.h"

namespace stappler::xenolith::gl {

Device::Device() { }

Device::~Device() {
	invalidateObjects();
}

bool Device::init(const Instance *instance) {
	_glInstance = instance;
	_samplersInfo.emplace_back(SamplerInfo(Filter::Nearest, Filter::Nearest));
	_samplersInfo.emplace_back(SamplerInfo{Filter::Linear, Filter::Linear});
	return true;
}

void Device::end() {
	_started = false;

	if (isRetainTrackerEnabled()) {
		log::vtext("Gl-Device", "Backtrace for ", (void *)this);
		foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) {
				stream << "\t" << it << "\n";
			}
			log::text("Gl-Device-Backtrace", stream.str());
		});
	}
}

Rc<Shader> Device::getProgram(StringView name) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(name);
	if (it != _shaders.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<Shader> Device::addProgram(Rc<Shader> program) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(program->getName());
	if (it == _shaders.end()) {
		_shaders.emplace(program->getName().str<Interface>(), program);
		return program;
	} else {
		return it->second;
	}
}

Rc<Framebuffer> Device::makeFramebuffer(const renderqueue::PassData *, SpanView<Rc<gl::ImageView>>, Extent2) {
	return nullptr;
}

auto Device::makeImage(const ImageInfo &) -> Rc<ImageStorage> {
	return nullptr;
}

Rc<Semaphore> Device::makeSemaphore() {
	return nullptr;
}

Rc<ImageView> Device::makeImageView(const Rc<ImageObject> &, const ImageViewInfo &) {
	return nullptr;
}

/*void Device::compileResource(gl::Loop &, const Rc<Resource> &req, Function<void(bool)> &&complete) {

}

void Device::compileRenderQueue(gl::Loop &loop, const Rc<RenderQueue> &queue, Function<void(bool)> &&complete) {

}

void Device::compileMaterials(gl::Loop &loop, Rc<MaterialInputData> &&) {

}*/

void Device::addObject(ObjectInterface *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.emplace(obj);
}

void Device::removeObject(ObjectInterface *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.erase(obj);
}

void Device::onLoopStarted(gl::Loop &loop) {

}

void Device::onLoopEnded(gl::Loop &) {

}

bool Device::supportsUpdateAfterBind(DescriptorType) const {
	return false;
}

void Device::clearShaders() {
	_shaders.clear();
}

void Device::invalidateObjects() {
	auto objs = std::move(_objects);
	_objects.clear();
	for (auto &it : objs) {
		if (auto ref = dynamic_cast<Ref *>(it)) {
			log::vtext("Gl-Device", "Object ", (void *)it, " (", typeid(*it).name(),
					") [rc:", ref->getReferenceCount(), "] was not destroyed before device destruction");
		} else {
			log::vtext("Gl-Device", "Object ", (void *)it, " (", typeid(*it).name(),
					") was not destroyed before device destruction");
		}
		if (auto ref = dynamic_cast<const Ref *>(it)) {
			log::vtext("Gl-Device", "Backtrace for ", (void *)it);
			ref->foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
				StringStream stream;
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) {
					stream << "\t" << it << "\n";
				}
				log::text("Gl-Device-Backtrace", stream.str());
			});
		}
		it->invalidate();
	}
}

}
