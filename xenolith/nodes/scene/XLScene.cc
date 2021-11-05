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

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2(_contentSize) / 2.0f);

	log::vtext("Scene", "ContentSize: ", _contentSize);
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size::ZERO) {
		setContentSize(dir->getScreenSize());
	}

	dir->getResourceCache()->addResource(_queue->getInternalResource());
	if (_materials.empty()) {
		readInitialMaterials();
	}

	onEnter(this);
}

void Scene::onFinished(Director *dir) {
	dir->getResourceCache()->removeResource(_queue->getInternalResource()->getName());
	if (_director == dir) {
		_director = nullptr;
	}
}

void Scene::onFrameStarted(gl::FrameHandle &) {

}

void Scene::onFrameEnded(gl::FrameHandle &) {

}

void Scene::on2dVertexInput(gl::FrameHandle &frame, const Rc<gl::AttachmentHandle> &attachment) {
	_director->getApplication()->performOnMainThread([this, frame = Rc<gl::FrameHandle>(&frame), attachment = attachment] {
		RenderFrameInfo info;
		info.director = _director;
		info.scene = this;
		info.pool = frame->getPool()->getPool();
		info.transformStack.reserve(8);
		info.zPath.reserve(8);
		info.transformStack.push_back(_director->getGeneralProjection());
		info.commands = Rc<gl::CommandList>::create(frame->getPool());

		render(info);

		frame->submitInput(attachment, move(info.commands));
	}, this);
}

void Scene::onQueueEnabled(const gl::Swapchain *) {
	_refId = retain();
}

void Scene::onQueueDisabled() {
	release(_refId);
}

uint64_t Scene::getMaterial(const MaterialInfo &info) const {
	auto it = _materials.find(info.hash());
	if (it != _materials.end()) {
		for (auto &m : it->second) {
			if (m.first == info) {
				return m.second;
			}
		}
	}
	return 0;
}

Rc<gl::RenderQueue> Scene::makeQueue(gl::RenderQueue::Builder &&builder) {
	builder.setBeginCallback([this] (gl::FrameHandle &frame) {
		onFrameStarted(frame);
	});
	builder.setEndCallback([this] (gl::FrameHandle &frame) {
		onFrameEnded(frame);
	});
	builder.setEnableCallback([this] (const gl::Swapchain *swapchain) {
		onQueueEnabled(swapchain);
	});
	builder.setDisableCallback([this] () {
		onQueueDisabled();
	});

	return Rc<gl::RenderQueue>::create(move(builder));
}

void Scene::readInitialMaterials() {
	for (auto &it : _queue->getAttachments()) {
		if (auto a = dynamic_cast<gl::MaterialAttachment *>(it.get())) {
			for (auto &m : a->getInitialMaterials()) {
				MaterialInfo info = getMaterialInfo(a->getType(), m);
				auto materialHash = info.hash();
				auto it = _materials.find(info.hash());
				if (it != _materials.end()) {
					it->second.emplace_back(info, m->getId());
				} else {
					_materials.emplace(materialHash,
							Vector<Pair<MaterialInfo, uint64_t>>({pair(info, m->getId())}));
				}
			}
		}
	}
}

MaterialInfo Scene::getMaterialInfo(gl::MaterialType type, const Rc<gl::Material> &material) const {
	MaterialInfo ret;
	ret.type = type;

	size_t idx = 0;
	for (auto &it : material->getImages()) {
		if (idx < config::MaxMaterialImages) {
			ret.images[idx] = it.image->image->getIndex();
			ret.samplers[idx] = it.sampler;
		}
		++ idx;
	}

	return ret;
}

}
