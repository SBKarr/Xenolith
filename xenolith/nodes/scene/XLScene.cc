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
#include "XLGlLoop.h"
#include "XLRenderQueueFrameHandle.h"
#include "XLVkMaterialRenderPass.h"

namespace stappler::xenolith {

Scene::~Scene() {
	_queue = nullptr;
}

bool Scene::init(Application *app, RenderQueue::Builder &&builder) {
	if (!Node::init()) {
		return false;
	}

	setLocalZOrder(ZOrderTransparent);

	_application = app;
	_queue = makeQueue(move(builder));

	if (!isnan(app->getData().density)) {
		setDensity(app->getData().density);
	}

	return true;
}

bool Scene::init(Application *app, RenderQueue::Builder &&builder, Size2 size, float density) {
	if (!Node::init()) {
		return false;
	}

	_application = app;
	_queue = makeQueue(move(builder));

	if (!isnan(density)) {
		setDensity(density);
	}

	setContentSize(size / _density);

	return true;
}

void Scene::renderRequest(const Rc<FrameRequest> &req) {
	if (!_director) {
		return;
	}

	RenderFrameInfo info;
	info.pool = req->getPool()->getPool();
	info.shadows = Rc<gl::CommandList>::create(req->getPool());
	info.commands = Rc<gl::CommandList>::create(req->getPool());
	info.commands->setStatCallback([dir = Rc<Director>(_director)] (gl::DrawStat stat) {
		dir->getApplication()->performOnMainThread([dir = move(dir), stat] {
			dir->pushDrawStat(stat);
		});
	});

	render(info);

	_director->getView()->getLoop()->performOnGlThread([req, a = _bufferAttachment, commands = move(info.commands)] () mutable {
		req->addInput(a, move(commands));
	}, req);

	// submit material updates
	if (!_pending.empty()) {
		for (auto &it : _pending) {
			Vector<Rc<renderqueue::DependencyEvent>> events;
			if (_materialDependency) {
				events.emplace_back(_materialDependency);
			}

			if (!it.second.toAdd.empty() || !it.second.toRemove.empty()) {
				auto req = Rc<gl::MaterialInputData>::alloc();
				req->attachment = it.first;
				req->materialsToAddOrUpdate = move(it.second.toAdd);
				req->materialsToRemove = move(it.second.toRemove);

				for (auto &it : req->materialsToRemove) {
					emplace_ordered(_revokedIds, it);
				}

				_director->getView()->getLoop()->compileMaterials(move(req), events);
			}
		}
		_pending.clear();
		_materialDependency = nullptr;
	}
}

void Scene::render(RenderFrameInfo &info) {
	info.director = _director;
	info.scene = this;
	info.zPath.reserve(8);
	info.currentStateId = 0;

	info.viewProjectionStack.reserve(2);
	info.viewProjectionStack.push_back(_director->getGeneralProjection());

	info.modelTransformStack.reserve(8);
	info.modelTransformStack.push_back(Mat4::IDENTITY);

	auto eventDispatcher = _director->getInputDispatcher();

	info.input = eventDispatcher->acquireNewStorage();

	visitGeometry(info, NodeFlags::None);
	visitDraw(info, NodeFlags::None);

	if (_materialDependency) {
		emplace_ordered(info.commands->waitDependencies, Rc<renderqueue::DependencyEvent>(_materialDependency));
	}

	eventDispatcher->commitStorage(move(info.input));
}

void Scene::onContentSizeDirty() {
	Node::onContentSizeDirty();

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2((_contentSize * _density) / 2.0f));

	log::vtext("Scene", "ContentSize: ", _contentSize, " density: ", _density);
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size2::ZERO) {
		setContentSize(dir->getScreenSize() / _density);
	}

	if (auto res = _queue->getInternalResource()) {
		dir->getResourceCache()->addResource(res);
	}
	if (_materials.empty()) {
		for (auto &it : _queue->getAttachments()) {
			if (auto a = dynamic_cast<gl::MaterialAttachment *>(it.get())) {
				readInitialMaterials(a);
			}
			if (auto a = dynamic_cast<vk::VertexMaterialAttachment *>(it.get())) {
				_bufferAttachment = a;
			}
		}
	}

	onEnter(this);
}

void Scene::onFinished(Director *dir) {
	onExit();

	if (_director == dir) {
		if (auto res = _queue->getInternalResource()) {
			dir->getResourceCache()->removeResource(res->getName());
		}
		_attachmentsByType.clear();
		_materials.clear();
		_pending.clear();
		_materialDependency = nullptr;

		_director = nullptr;
	}
}

void Scene::onFrameStarted(FrameRequest &req) {
	req.setSceneId(retain());
}

void Scene::onFrameEnded(FrameRequest &req) {
	release(req.getSceneId());
}

void Scene::on2dVertexInput(FrameQueue &frame, const Rc<renderqueue::AttachmentHandle> &attachment, Function<void(bool)> &&cb) {
	if (!_director) {
		cb(false);
		return;
	}

	auto handle = frame.getFrame();

	_director->getApplication()->performOnMainThread(
			[this, handle, frame = Rc<renderqueue::FrameQueue>(&frame), attachment, cb = move(cb)] () mutable {
		if (!_director) {
			return;
		}

		RenderFrameInfo info;
		info.pool = frame->getPool()->getPool();
		info.commands = Rc<gl::CommandList>::create(frame->getPool());
		info.commands->setStatCallback([dir = Rc<Director>(_director)] (gl::DrawStat stat) {
			dir->getApplication()->performOnMainThread([dir = move(dir), stat] {
				dir->pushDrawStat(stat);
			});
		});

		render(info);

		handle->performOnGlThread([attachment, commands = move(info.commands), cb = move(cb), frame = Rc<renderqueue::FrameQueue>(frame)] (FrameHandle &) mutable {
			attachment->submitInput(*frame, move(commands), move(cb));
		}, handle, false, "Scene::on2dVertexInput");

		// submit material updates
		if (!_pending.empty()) {
			for (auto &it : _pending) {
				Vector<Rc<renderqueue::DependencyEvent>> events;
				if (_materialDependency) {
					events.emplace_back(_materialDependency);
				}

				if (!it.second.toAdd.empty() || !it.second.toRemove.empty()) {
					auto req = Rc<gl::MaterialInputData>::alloc();
					req->attachment = it.first;
					req->materialsToAddOrUpdate = move(it.second.toAdd);
					req->materialsToRemove = move(it.second.toRemove);

					for (auto &it : req->materialsToRemove) {
						emplace_ordered(_revokedIds, it);
					}

					frame->getLoop()->compileMaterials(move(req), events);
				}
			}
			_pending.clear();
			_materialDependency = nullptr;
		}
	}, this);
}

uint64_t Scene::getMaterial(const MaterialInfo &info) const {
	auto it = _materials.find(info.hash());
	if (it != _materials.end()) {
		for (auto &m : it->second) {
			if (m.info == info) {
				return m.id;
			}
		}
	}
	return 0;
}

uint64_t Scene::acquireMaterial(const MaterialInfo &info, Vector<gl::MaterialImage> &&images, bool revokable) {
	auto aIt = _attachmentsByType.find(info.type);
	if (aIt == _attachmentsByType.end()) {
		return 0;
	}

	auto &a = aIt->second;

	auto pipeline = getPipelineForMaterial(a, info);
	if (!pipeline) {
		return 0;
	}

	for (uint32_t idx = 0; idx < uint32_t(images.size()); ++ idx) {
		if (images[idx].image != nullptr) {
			auto &image = images[idx];
			image.info = getImageViewForMaterial(info, idx, images[idx].image);
			image.view = nullptr;
			image.sampler = info.samplers[idx];
		}
	}

	gl::MaterialId newId = 0;
	if (revokable) {
		if (!_revokedIds.empty()) {
			newId = _revokedIds.back();
			_revokedIds.pop_back();
		}
	}

	if (newId == 0) {
		newId = aIt->second.attachment->getNextMaterialId();
	}

	if (auto m = Rc<gl::Material>::create(newId, pipeline, move(images), getDataForMaterial(a.attachment, info))) {
		auto id = m->getId();
		addPendingMaterial(a.attachment, move(m));
		addMaterial(info, id, revokable);
		return id;
	}
	return 0;
}

void Scene::setDensity(float density) {
	if (_density != density) {
		_density = density;
		setScale(_density);
		_contentSizeDirty = true;

		auto diff = _contentSize / _density;

		setPosition(Vec2(diff / 2.0f));
	}
}

void Scene::revokeImages(const Vector<uint64_t> &vec) {
	Vector<uint32_t> tmpRevoke2d;
	Vector<uint32_t> tmpRevoke3d;

	Vector<uint32_t> *revoke2d = &tmpRevoke2d;
	Vector<uint32_t> *revoke3d = &tmpRevoke3d;

	for (auto &it : _attachmentsByType) {
		switch (it.first) {
		case gl::MaterialType::Basic2D: {
			auto iit = _pending.find(it.second.attachment);
			if (iit == _pending.end()) {
				iit = _pending.emplace(it.second.attachment, PendingData()).first;
			}
			revoke2d = &iit->second.toRemove;
			break;
		}
		case gl::MaterialType::Basic3D: {
			auto iit = _pending.find(it.second.attachment);
			if (iit == _pending.end()) {
				iit = _pending.emplace(it.second.attachment,PendingData()).first;
			}
			revoke3d = &iit->second.toRemove;
			break;
		}
		}
	}

	auto shouldRevoke = [&] (const SceneMaterialInfo &iit) {
		for (auto &id : vec) {
			if (iit.info.hasImage(id)) {
				switch (iit.info.type) {
				case gl::MaterialType::Basic2D: emplace_ordered(*revoke2d, iit.id); break;
				case gl::MaterialType::Basic3D: emplace_ordered(*revoke3d, iit.id); break;
				}
				return true;
			}
		}
		return false;
	};

	for (auto &it : _materials) {
		auto iit = it.second.begin();
		while (iit != it.second.end()) {
			if (iit->revokable) {
				if (shouldRevoke(*iit)) {
					iit = it.second.erase(iit);
				} else {
					++ iit;
				}
			} else {
				++ iit;
			}
		}
	}
}

auto Scene::makeQueue(RenderQueue::Builder &&builder) -> Rc<RenderQueue> {
	builder.setBeginCallback([this] (FrameRequest &frame) {
		onFrameStarted(frame);
	});
	builder.setEndCallback([this] (FrameRequest &frame) {
		onFrameEnded(frame);
	});

	return Rc<RenderQueue>::create(move(builder));
}

void Scene::readInitialMaterials(gl::MaterialAttachment *a) {
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

			for (auto &pipeline : it->graphicPipelines) {
				auto hash = pipeline->material.hash();
				auto it = sp.pipelines.find(hash);
				if (it == sp.pipelines.end()) {
					it = sp.pipelines.emplace(hash, Vector<const PipelineData *>()).first;
				}
				it->second.emplace_back(pipeline);
				log::vtext("Scene", "Pipeline ", pipeline->material.description(), " : ", pipeline->material.data());
			}
		}

		renderPass = a->getPrevRenderPass(renderPass);
	}
	for (auto &m : a->getInitialMaterials()) {
		addMaterial(getMaterialInfo(a->getType(), m), m->getId(), false);
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

auto Scene::getPipelineForMaterial(const AttachmentData &a, const MaterialInfo &info) const -> const PipelineData * {
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

bool Scene::isPipelineMatch(const PipelineInfo *data, const MaterialInfo &info) const {
	return true; // TODO: true match
}

void Scene::addPendingMaterial(const gl::MaterialAttachment *a, Rc<gl::Material> &&material) {
	auto it = _pending.find(a);
	if (it == _pending.end()) {
		it = _pending.emplace(a, PendingData()).first;
	}
	it->second.toAdd.emplace_back(move(material));
	if (!_materialDependency) {
		_materialDependency = Rc<renderqueue::DependencyEvent>::alloc();
	}
}

void Scene::addMaterial(const MaterialInfo &info, gl::MaterialId id, bool revokable) {
	auto materialHash = info.hash();
	auto it = _materials.find(info.hash());
	if (it != _materials.end()) {
		it->second.emplace_back(SceneMaterialInfo{info, id, revokable});
	} else {
		Vector<SceneMaterialInfo> ids({SceneMaterialInfo{info, id, revokable}});
		_materials.emplace(materialHash, move(ids));
	}
}

void Scene::listMaterials() const {
	for (auto &it : _materials) {
		std::cout << it.first << ":\n";
		for (auto &iit : it.second) {
			std::cout << "\t" << iit.info.description() << " -> " << iit.id << "\n";
		}
	}
}

}
