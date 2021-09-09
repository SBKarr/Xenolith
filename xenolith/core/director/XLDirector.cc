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

#include "XLDirector.h"
#include "XLGlView.h"
#include "XLGlDevice.h"
#include "XLScene.h"
#include "XLDefaultShaders.h"
#include "XLVertexArray.h"

#include "XLVkImageAttachment.h"
#include "XLVkBufferAttachment.h"
#include "XLVkRenderPass.h"

namespace stappler::xenolith {

XL_DECLARE_EVENT_CLASS(Director, onProjectionChanged);
XL_DECLARE_EVENT_CLASS(Director, onAfterUpdate);
XL_DECLARE_EVENT_CLASS(Director, onAfterVisit);
XL_DECLARE_EVENT_CLASS(Director, onAfterDraw);

Rc<Director> Director::getInstance() {
	return Application::getInstance()->getDirector();
}

Director::Director() { }

Director::~Director() { }

bool Director::init() {
	_resourceCache = Rc<ResourceCache>::create(this);
	return true;
}

void Director::setView(gl::View *view) {
	if (view != _view) {
		_view = view;

		if (_sizeChangedEvent) {
			removeHandlerNode(_sizeChangedEvent);
			_sizeChangedEvent = nullptr;
		}

		_sizeChangedEvent = onEventWithObject(gl::View::onScreenSize, view, [&] (const Event &) {
			if (_scene) {
				auto &size = _view->getScreenSize();
				auto d = _view->getDensity();

				_scene->setContentSize(size / d);

				updateGeneralTransform();
			}
		});

		updateGeneralTransform();
	}
}

void Director::update(uint64_t) {

}

bool Director::mainLoop(uint64_t t) {
	if (_nextScene) {
		if (_scene) {
			_scene->onExit();
		}
		_scene = _nextScene;

		auto &size = _view->getScreenSize();
		auto d = _view->getDensity();

		_scene->setContentSize(size / d);
		_scene->onEnter();
		_nextScene = nullptr;
	}

	update(t);

	return true;
}

void Director::render(const Rc<gl::FrameHandle> &frame) {
	for (auto &it : frame->getInputAttachments()) {
		if (it->getAttachment()->getName() == "VertexInput") {
			VertexArray array;
			array.init(4, 6);

			auto quad = array.addQuad();
			quad.setGeometry(Vec4(-0.5f, -0.5f, 0, 0), Size(1.0f, 1.0f));
			quad.setColor({
				Color::Red_500,
				Color::Green_500,
				Color::Blue_500,
				Color::White
			});

			frame->submitInput(it, Rc<gl::VertexData>(array.pop()));
		} else {
			log::vtext("Director", "Unknown attachment: ", it->getAttachment()->getName());
		}
	}
}

Rc<gl::DrawScheme> Director::construct() {
	if (!_scene) {
		return nullptr;
	}

	auto df = gl::DrawScheme::create();

	auto p = df->getPool();
	memory::pool::push(p);

	do {
		RenderFrameInfo info;
		info.director = this;
		info.scene = _scene;
		info.pool = df->getPool();
		info.scheme = df.get();
		info.transformStack.reserve(8);
		info.zPath.reserve(8);

		info.transformStack.push_back(_generalProjection);

		_scene->render(info);
	} while (0);

	memory::pool::pop();

	return df;
}

void Director::end() {
	if (_scene) {
		_scene->onExit();
		_nextScene = _scene;
		_scene = nullptr;
	}
	_view = nullptr;
}

Size Director::getScreenSize() const {
	return _view->getScreenSize();
}

Rc<ResourceCache> Director::getResourceCache() const {
	return _resourceCache;
}

void Director::runScene(Rc<Scene> scene) {
	_nextScene = scene;
}

Rc<gl::RenderQueue> Director::onDefaultRenderQueue(const gl::ImageInfo &info) {
	gl::RenderQueue::Builder builder("DefaultRenderQueue", gl::RenderQueue::Continuous);
	auto defaultFrag = builder.addProgramByRef("DefaultRenderQueue_DefaultVert", gl::ProgramStage::Vertex, shaders::DefaultVert);
	auto defaultVert = builder.addProgramByRef("DefaultRenderQueue_DefaultFrag", gl::ProgramStage::Fragment, shaders::DefaultFrag);
	auto vertexFrag = builder.addProgramByRef("DefaultRenderQueue_VertexVert", gl::ProgramStage::Vertex, shaders::VertexVert);
	auto vertexVert = builder.addProgramByRef("DefaultRenderQueue_VertexFrag", gl::ProgramStage::Fragment, shaders::VertexFrag);

	auto out = Rc<vk::SwapchainAttachment>::create("Swapchain", gl::ImageInfo(info),
			gl::AttachmentLayout::Undefined, gl::AttachmentLayout::PresentSrc);

	auto input = Rc<vk::VertexBufferAttachment>::create("VertexInput", gl::BufferInfo(gl::BufferUsage::StorageBuffer, gl::ProgramStage::Vertex));

	auto pass = Rc<vk::VertexRenderPass>::create("SwapchainPass", gl::RenderOrderingHighest);
	builder.addRenderPass(pass);
	builder.addPipeline(pass, "Default", Vector<const gl::ProgramData *>({
		defaultVert,
		defaultFrag
	}));
	builder.addPipeline(pass, "Vertexes", Vector<const gl::ProgramData *>({
		vertexVert,
		vertexFrag
	}));

	/*builder.addSubpassDependency(pass,
			gl::RenderSubpassDependency::External, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::None,
			0, gl::PipelineStage::ColorAttachmentOutput, gl::AccessType::ColorAttachmentWrite, false);*/

	builder.addPassInput(pass, 0, input);
	builder.addPassOutput(pass, 0, out);

	builder.addInput(input);
	builder.addOutput(out);

	return Rc<gl::RenderQueue>::create(move(builder));
}

void Director::invalidate() {

}

static inline int32_t sp_gcd (int16_t a, int16_t b) {
	int32_t c;
	while ( a != 0 ) {
		c = a; a = b%a;  b = c;
	}
	return b;
}

void Director::updateGeneralTransform() {
	auto d = _view->getDensity();
	auto size = _view->getScreenSize() / d;
	// General (2D) transform
	int32_t gcd = sp_gcd(size.width, size.height);
	int32_t dw = (int32_t)size.width / gcd;
	int32_t dh = (int32_t)size.height / gcd;
	int32_t dwh = gcd * dw * dh;

	float mod = 1.0f;
	while (dwh * mod > 16384) {
		mod /= 2.0f;
	}

	Mat4 proj;
	proj.scale(dh * mod, dw * mod, -1.0);
	proj.m[12] = -dwh * mod / 2.0f;
	proj.m[13] = -dwh * mod / 2.0f;
	proj.m[14] = dwh * mod / 2.0f - 1;
	proj.m[15] = dwh * mod / 2.0f + 1;
	proj.m[11] = -1.0f;

	_generalProjection = proj;
}

}
