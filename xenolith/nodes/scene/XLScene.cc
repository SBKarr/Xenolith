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

#include "XLScene.h"
#include "XLDirector.h"

namespace stappler::xenolith {

Scene::~Scene() {
	_queue = nullptr;
}

bool Scene::init(gl::RenderQueue::Builder &&builder) {
	if (!Node::init()) {
		return false;
	}

	_queue = makeQueue(move(builder));

	return true;
}

bool Scene::init(gl::RenderQueue::Builder &&builder, Size size) {
	if (!Node::init()) {
		return false;
	}

	_queue = makeQueue(move(builder));
	setContentSize(size);

	return true;
}

void Scene::render(RenderFrameInfo &info) {
	visit(info, NodeFlags::None);
}

void Scene::onContentSizeDirty() {
	Node::onContentSizeDirty();

	log::vtext("Scene", "ContentSize: ", _contentSize);
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size::ZERO) {
		setContentSize(dir->getScreenSize());
	}
}

void Scene::onFinished(Director *dir) {
	if (_director == dir) {
		_director = nullptr;
	}
}

void Scene::onFrameStarted(gl::FrameHandle &) {

}

void Scene::onFrameEnded(gl::FrameHandle &) {

}

void Scene::onFrameInput(gl::FrameHandle &frame, const Rc<gl::AttachmentHandle> &attachment) {
	_director->getApplication()->performOnMainThread([this, frame = Rc<gl::FrameHandle>(&frame), attachment = attachment] {
		_director->acquireInput(*frame, attachment);
	}, this);
}

void Scene::onQueueEnabled(const gl::Swapchain *) {
	_refId = retain();
}

void Scene::onQueueDisabled() {
	release(_refId);
}

Rc<gl::RenderQueue> Scene::makeQueue(gl::RenderQueue::Builder &&builder) {
	builder.setBeginCallback([this] (gl::FrameHandle &frame) {
		onFrameStarted(frame);
	});
	builder.setEndCallback([this] (gl::FrameHandle &frame) {
		onFrameEnded(frame);
	});
	builder.setInputCallback([this] (gl::FrameHandle &frame, const Rc<gl::AttachmentHandle> &a) {
		onFrameInput(frame, a);
	});
	builder.setEnableCallback([this] (const gl::Swapchain *swapchain) {
		onQueueEnabled(swapchain);
	});
	builder.setDisableCallback([this] () {
		onQueueDisabled();
	});

	return Rc<gl::RenderQueue>::create(move(builder));
}

}
