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

#include "XLInputDispatcher.h"
#include "XLDirector.h"
#include "XLApplication.h"

namespace stappler::xenolith {

Scene::~Scene() {
	_queue = nullptr;
}

bool Scene::init(Application *app, gl::RenderQueue::Builder &&builder) {
	if (!Node::init()) {
		return false;
	}

	_application = app;
	_queue = makeQueue(move(builder));

	return true;
}

bool Scene::init(Application *app, gl::RenderQueue::Builder &&builder, Size2 size) {
	if (!Node::init()) {
		return false;
	}

	_application = app;
	_queue = makeQueue(move(builder));
	setContentSize(size);

	return true;
}

void Scene::render(RenderFrameInfo &info) {
	info.director = _director;
	info.scene = this;
	info.zPath.reserve(8);

	info.viewProjectionStack.reserve(2);
	info.viewProjectionStack.push_back(_director->getGeneralProjection());

	info.modelTransformStack.reserve(8);
	info.modelTransformStack.push_back(Mat4::IDENTITY);

	auto eventDispatcher = _director->getInputDispatcher();

	info.input = eventDispatcher->acquireNewStorage();

	visit(info, NodeFlags::None);

	eventDispatcher->commitStorage(move(info.input));
}

void Scene::onContentSizeDirty() {
	Node::onContentSizeDirty();

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2(_contentSize) / 2.0f);

	log::vtext("Scene", "ContentSize: ", _contentSize);
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size2::ZERO) {
		setContentSize(dir->getScreenSize());
	}

	if (auto res = _queue->getInternalResource()) {
		dir->getResourceCache()->addResource(res);
	}
	if (_materials.empty()) {
		readInitialMaterials();
	}

	onEnter(this);
}

void Scene::onFinished(Director *dir) {
	onExit();

	if (_director == dir) {
		if (auto res = _queue->getInternalResource()) {
			dir->getResourceCache()->removeResource(res->getName());
		}
		_director = nullptr;
	}
}

void Scene::onFrameStarted(gl::FrameRequest &req) {
	req.setSceneId(retain());
}

void Scene::onFrameEnded(gl::FrameRequest &req) {
	release(req.getSceneId());
}

void Scene::on2dVertexInput(gl::FrameQueue &frame, const Rc<gl::AttachmentHandle> &attachment, Function<void(bool)> &&cb) {
	if (!_director) {
		cb(false);
		return;
	}

	_director->getApplication()->performOnMainThread(
			[this, frame = Rc<gl::FrameQueue>(&frame), attachment = attachment, cb = move(cb)] () mutable {
		if (!_director) {
			return;
		}

		RenderFrameInfo info;
		info.pool = frame->getPool()->getPool();
		info.commands = Rc<gl::CommandList>::create(frame->getPool());

		render(info);

		attachment->submitInput(*frame, move(info.commands), move(cb));

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

uint64_t Scene::acquireMaterial(const MaterialInfo &info, Vector<gl::MaterialImage> &&images) {
	auto aIt = _attachmentsByType.find(info.type);
	if (aIt == _attachmentsByType.end()) {
		return 0;
	}

	auto &a = aIt->second;

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

	if (auto m = Rc<gl::Material>::create(pipeline, move(images), getDataForMaterial(a.attachment, info))) {
		auto id = m->getId();
		addPendingMaterial(a.attachment, move(m));
		addMaterial(info, id);
		return id;
	}
	return 0;
}

Rc<gl::RenderQueue> Scene::makeQueue(gl::RenderQueue::Builder &&builder) {
	builder.setBeginCallback([this] (gl::FrameRequest &frame) {
		onFrameStarted(frame);
	});
	builder.setEndCallback([this] (gl::FrameRequest &frame) {
		onFrameEnded(frame);
	});

	return Rc<gl::RenderQueue>::create(move(builder));
}

void Scene::readInitialMaterials() {
	for (auto &it : _queue->getAttachments()) {
		if (auto a = dynamic_cast<gl::MaterialAttachment *>(it.get())) {
			auto &v = _attachmentsByType.emplace(a->getType(), AttachmentData({a})).first->second;

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

					auto &sp = v.subasses.emplace_back(SubpassData({&(*it)}));

					for (auto &pipeline : it->pipelines) {
						auto hash = pipeline->material.hash();
						auto it = sp.pipelines.find(hash);
						if (it == sp.pipelines.end()) {
							it = sp.pipelines.emplace(hash, Vector<const gl::PipelineData *>()).first;
						}
						it->second.emplace_back(pipeline);
						log::vtext("Scene", "Pipeline ", pipeline->material.description(), " : ", pipeline->material.data());
					}
				}

				renderPass = a->getPrevRenderPass(renderPass);
			}
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
			ret.colorModes[idx] = it.info.getColorMode();
		}
		++ idx;
	}

	ret.pipeline = material->getPipeline()->material;

	return ret;
}

gl::ImageViewInfo Scene::getImageViewForMaterial(const MaterialInfo &info, uint32_t idx, const gl::ImageData *image) const {
	return gl::ImageViewInfo(image->format, info.colorModes[idx]);
}

Bytes Scene::getDataForMaterial(const gl::MaterialAttachment *a, const MaterialInfo &) const {
	return Bytes();
}

const gl::PipelineData *Scene::getPipelineForMaterial(const AttachmentData &a, const MaterialInfo &info) const {
	auto hash = info.pipeline.hash();
	for (auto &it : a.subasses) {
		auto hashIt = it.pipelines.find(hash);
		if (hashIt != it.pipelines.end()) {
			for (auto &pipeline : hashIt->second) {
				if (pipeline->material == info.pipeline && isPipelineMatch(pipeline, info)) {
					return pipeline;
				}
			}
		}
	}
	log::vtext("Scene", "No pipeline for attachment '", a.attachment->getName(), "': ",
			info.pipeline.description(), " : ", info.pipeline.data());
	return nullptr;
}

bool Scene::isPipelineMatch(const gl::PipelineInfo *data, const MaterialInfo &info) const {
	return true; // TODO: true match
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
