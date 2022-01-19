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

		// submit material updates
		if (!_pendingMaterials.empty()) {
			for (auto &it : _pendingMaterials) {
				auto req = Rc<gl::MaterialInputData>::alloc();
				req->attachment = it.first;
				req->materialsToAddOrUpdate = move(it.second);
				frame->getLoop()->compileMaterials(req);
			}
			_pendingMaterials.clear();
		}
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

/*uint64_t Scene::acquireMaterial(const MaterialInfo &info, const Vector<const gl::ImageData *> &images) {
	if (auto a = getAttachmentByType(info.type)) {
		auto pipeline = getPipelineForMaterial(a, info);
		if (!pipeline) {
			return 0;
		}

		Vector<gl::MaterialImage> imgs;
		for (size_t idx = 0; idx < images.size(); ++ idx) {
			if (images[idx] != nullptr) {
				gl::MaterialImage image;
				image.image = images[idx];
				image.info = getImageViewForMaterial(info, idx, images[idx]);
				image.view = nullptr;
				image.sampler = info.samplers[idx];
				imgs.emplace_back(move(image));
			}
		}
		if (auto m = Rc<gl::Material>::create(pipeline, move(imgs), getDataForMaterial(a, info))) {
			auto id = m->getId();
			addPendingMaterial(a, move(m));
			addMaterial(info, id);
			return id;
		}
	}
	return 0;
}*/

uint64_t Scene::acquireMaterial(const MaterialInfo &info, Vector<gl::MaterialImage> &&images) {
	if (auto a = getAttachmentByType(info.type)) {
		auto pipeline = getPipelineForMaterial(a, info);
		if (!pipeline) {
			return 0;
		}

		for (size_t idx = 0; idx < images.size(); ++ idx) {
			if (images[idx].image != nullptr) {
				auto &image = images[idx];
				image.info = getImageViewForMaterial(info, idx, images[idx].image);
				image.view = nullptr;
				image.sampler = info.samplers[idx];
			}
		}

		if (auto m = Rc<gl::Material>::create(pipeline, move(images), getDataForMaterial(a, info))) {
			auto id = m->getId();
			addPendingMaterial(a, move(m));
			addMaterial(info, id);
			return id;
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
			_attachmentsByType.emplace(a->getType(), a);
			for (auto &m : a->getInitialMaterials()) {
				addMaterial(getMaterialInfo(a->getType(), m), m->getId());
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

gl::ImageViewInfo Scene::getImageViewForMaterial(const MaterialInfo &info, uint32_t idx, const gl::ImageData *image) const {
	gl::ImageViewInfo ret;
	switch (info.colorMode.getMode()) {
	case ColorMode::Solid: {
		auto format = gl::getImagePixelFormat(image->format);
		switch (format) {
		case gl::PixelFormat::Unknown: break;
		case gl::PixelFormat::A:
			ret.r = gl::ComponentMapping::R;
			ret.g = gl::ComponentMapping::R;
			ret.b = gl::ComponentMapping::R;
			ret.a = gl::ComponentMapping::One;
			/*ret.r = gl::ComponentMapping::One;
			ret.g = gl::ComponentMapping::One;
			ret.b = gl::ComponentMapping::One;
			ret.a = gl::ComponentMapping::R;*/
			break;
		case gl::PixelFormat::IA:
			ret.r = gl::ComponentMapping::B;
			ret.g = gl::ComponentMapping::B;
			ret.b = gl::ComponentMapping::B;
			ret.a = gl::ComponentMapping::G;
			break;
		case gl::PixelFormat::RGB:
			ret.r = gl::ComponentMapping::Identity;
			ret.g = gl::ComponentMapping::Identity;
			ret.b = gl::ComponentMapping::Identity;
			ret.a = gl::ComponentMapping::One;
			break;
		case gl::PixelFormat::RGBA:
		case gl::PixelFormat::D:
		case gl::PixelFormat::DS:
		case gl::PixelFormat::S:
			ret.r = gl::ComponentMapping::Identity;
			ret.g = gl::ComponentMapping::Identity;
			ret.b = gl::ComponentMapping::Identity;
			ret.a = gl::ComponentMapping::Identity;
			break;
		}

		break;
	}
	case ColorMode::Custom:
		ret.r = info.colorMode.getR();
		ret.g = info.colorMode.getG();
		ret.b = info.colorMode.getB();
		ret.a = info.colorMode.getA();
		break;
	}
	return ret;
}

Bytes Scene::getDataForMaterial(const gl::MaterialAttachment *a, const MaterialInfo &) const {
	return Bytes();
}

const gl::PipelineData *Scene::getPipelineForMaterial(const gl::MaterialAttachment *a, const MaterialInfo &info) const {
	if (auto a = getAttachmentByType(info.type)) {
		auto renderPass = a->getLastRenderPass();
		while (renderPass) {
			auto &subpasses = renderPass->subpasses;

			for (auto it = subpasses.rbegin(); it != subpasses.rend(); ++ it) {
				// check if subpass has material attachment
				bool isUsable = false;
				for (auto &attachment : it->inputBuffers) {
					if (attachment->getAttachment() == a) {
						isUsable = true;
						break;
					}
				}

				if (!isUsable) {
					break;
				}

				for (auto &pipeline : it->pipelines) {
					if (isPipelineMatch(pipeline, info)) {
						return pipeline;
					}
				}
			}

			renderPass = a->getPrevRenderPass(renderPass);
		}
	}
	return nullptr;
}

bool Scene::isPipelineMatch(const gl::PipelineData *data, const MaterialInfo &info) const {
	return true; // TODO: true match
}

const gl::MaterialAttachment *Scene::getAttachmentByType(gl::MaterialType type) const {
	auto it = _attachmentsByType.find(type);
	if (it != _attachmentsByType.end()) {
		return it->second;
	}
	return nullptr;
}

void Scene::addPendingMaterial(const gl::MaterialAttachment *a, Rc<gl::Material> &&material) {
	auto it = _pendingMaterials.find(a);
	if (it != _pendingMaterials.end()) {
		it->second.emplace_back(move(material));
	} else {
		_pendingMaterials.emplace(a, Vector<Rc<gl::Material>>({move(material)}));
	}
}

void Scene::addMaterial(const MaterialInfo &info, gl::MaterialId id) {
	auto materialHash = info.hash();
	auto it = _materials.find(info.hash());
	if (it != _materials.end()) {
		it->second.emplace_back(info, id);
	} else {
		Vector<Pair<MaterialInfo, gl::MaterialId>> ids({pair(info, id)});
		_materials.emplace(materialHash, move(ids));
	}
}

}
