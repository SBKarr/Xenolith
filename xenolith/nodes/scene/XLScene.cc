/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLScene.h"

#include "XLInputDispatcher.h"
#include "XLDirector.h"
#include "XLApplication.h"
#include "XLGlLoop.h"
#include "XLRenderQueueFrameHandle.h"
#include "XLVkMaterialRenderPass.h"
#include "XLVkShadowRenderPass.h"

namespace stappler::xenolith {

Scene::~Scene() {
	_queue = nullptr;
}

bool Scene::init(Application *app, RenderQueue::Builder &&builder, const gl::FrameContraints &constraints) {
	if (!Node::init()) {
		return false;
	}

	setLocalZOrder(ZOrderTransparent);

	_application = app;
	_queue = makeQueue(move(builder));

	auto content = Rc<DynamicStateNode>::create();
	Node::addChildNode(content, 0, content->getTag());
	_content = content;

	setFrameConstraints(_constraints);

	return true;
}

void Scene::renderRequest(const Rc<FrameRequest> &req) {
	if (!_director) {
		return;
	}

	Size2 scaledExtent(_constraints.extent.width / _constraints.density, _constraints.extent.height / _constraints.density);

	RenderFrameInfo info;
	info.pool = req->getPool()->getPool();
	info.shadows = Rc<gl::CommandList>::create(req->getPool());
	info.commands = Rc<gl::CommandList>::create(req->getPool());
	info.commands->setStatCallback([dir = Rc<Director>(_director)] (gl::DrawStat stat) {
		dir->getApplication()->performOnMainThread([dir = move(dir), stat] {
			dir->pushDrawStat(stat);
		});
	});
	info.lights = Rc<gl::ShadowLightInput>::alloc();
	info.lights->sceneDensity = _constraints.density;
	info.lights->shadowDensity = _shadowDensity;
	info.lights->globalColor = _globalLight;

	for (auto &it : _lights) {
		switch (it->getType()) {
		case SceneLightType::Ambient:
			info.lights->addAmbientLight(Vec4(it->getNormal().x / scaledExtent.width, -it->getNormal().y / scaledExtent.height,
					it->getNormal().z, it->getNormal().w), it->getColor(), it->isSoftShadow());
			break;
		case SceneLightType::Direct:
			info.lights->addDirectLight(Vec4(it->getNormal().x / scaledExtent.width, -it->getNormal().y / scaledExtent.height,
					it->getNormal().z, it->getNormal().w), it->getColor(), it->getData());
			break;
		}
	}

	render(info);

	if (!info.commands->waitDependencies.empty()) {
		_application->scheduleUpdate();
	}

	_director->getView()->getLoop()->performOnGlThread(
			[req, q = _queue, dir = _director,
			 commands = move(info.commands),
			 shadows = move(info.shadows),
			 lights = move(info.lights)] () mutable {
		req->addInput(q->getInputAttachment<vk::VertexMaterialAttachment>(), move(commands));
		req->addInput(q->getInputAttachment<vk::ShadowLightDataAttachment>(), Rc<gl::ShadowLightInput>(lights));
		req->addInput(q->getInputAttachment<vk::ShadowVertexAttachment>(), move(shadows));
		req->addInput(q->getInputAttachment<vk::ShadowSdfImageAttachment>(), Rc<gl::ShadowLightInput>(lights));

		req->setOutput(q->getInputAttachment<vk::ShadowSdfImageAttachment>(), dir->getView(),
				[] (const Rc<gl::View> &view, renderqueue::FrameAttachmentData &data, bool success) {
			view->captureImage([] (const gl::ImageInfo &info, BytesView view) {
				Bitmap bmpSdf;
				bmpSdf.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::A8);

				Bitmap bmpHeight;
				bmpHeight.alloc(info.extent.width, info.extent.height, bitmap::PixelFormat::A8);

				auto d1 = bmpSdf.dataPtr();
				auto d2 = bmpHeight.dataPtr();

				while (!view.empty()) {
					auto value = view.readFloat16() / 16.0f;

					*d1 = uint8_t(value * 255.0f);
					++ d1;

					*d2 = uint8_t((view.readFloat16() / 20.0f) * 255.0f);
					++ d2;
				}

				bmpSdf.save(toString("sdf-", Time::now().toMicros(), ".png"));
				bmpHeight.save(toString("height-", Time::now().toMicros(), ".png"));

				std::cout << info.extent << "\n";
			}, data.image->getImage(), data.image->getLayout());
			return true;
		});
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

void Scene::onEnter(Scene *scene) {
	Node::onEnter(scene);

	for (auto &it : _lights) {
		it->onEnter(scene);
	}
}

void Scene::onExit() {
	for (auto &it : _lights) {
		it->onExit();
	}

	Node::onExit();
}

void Scene::onContentSizeDirty() {
	Node::onContentSizeDirty();

	setAnchorPoint(Anchor::Middle);
	setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

	_content->setPosition(Vec2(_constraints.contentPadding.left, _constraints.contentPadding.bottom) / _constraints.density);
	_content->setContentSize(Size2(_contentSize.width - _constraints.contentPadding.horizontal() / _constraints.density,
			_contentSize.height - _constraints.contentPadding.vertical() / _constraints.density));
	_content->setAnchorPoint(Anchor::BottomLeft);

	_cacheDirty = true;

	log::vtext("Scene", "ContentSize: ", _contentSize, " density: ", _constraints.density);
}

void Scene::onPresented(Director *dir) {
	_director = dir;
	if (getContentSize() == Size2::ZERO) {
		setContentSize(dir->getScreenSize() / _constraints.density);
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
			auto cache = dir->getResourceCache();
			if (cache) {
				cache->removeResource(res->getName());
			}
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

void Scene::setFrameConstraints(const gl::FrameContraints &constraints) {
	if (_constraints != constraints) {
		auto size = constraints.getScreenSize();

		_constraints = constraints;
		//_constraints.contentPadding = constraints.getRotatedPadding();
		setContentSize(size / _constraints.density);
		setScale(_constraints.density);
		_contentSizeDirty = true;

		setPosition(Vec2((_contentSize * _constraints.density) / 2.0f));

		auto contentPos = Vec2(_constraints.contentPadding.left, _constraints.contentPadding.bottom) / _constraints.density;
		auto contentSize = Size2(_contentSize.width - _constraints.contentPadding.horizontal() / _constraints.density,
				_contentSize.height - _constraints.contentPadding.vertical() / _constraints.density);

		_content->setPosition(contentPos);
		_content->setContentSize(contentSize);
		_content->setAnchorPoint(Anchor::BottomLeft);
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

void Scene::specializeRequest(const Rc<FrameRequest> &req) {
	if (auto a = _queue->getInputAttachment<vk::ShadowImageArrayAttachment>()) {
		gl::ImageInfoData info = a->getImageInfo();
		info.arrayLayers = gl::ArrayLayers(_lightsAmbientCount + _lightsDirectCount);
		info.extent =  Extent2(std::ceil(req->getFrameConstraints().extent.width * _shadowDensity),
				std::ceil(req->getFrameConstraints().extent.height * _shadowDensity));
		req->addImageSpecialization(a, move(info));

		if (_cachedShadowDensity != _shadowDensity / req->getFrameConstraints().density
				|| _cachedLightsCount != _lightsAmbientCount + _lightsDirectCount) {
			_cachedShadowDensity = _shadowDensity / req->getFrameConstraints().density;
			_cachedLightsCount = _lightsAmbientCount + _lightsDirectCount;
			req->setAttachmentsDirty(true);
		}
	}

	if (auto a = _queue->getInputAttachment<vk::ShadowSdfImageAttachment>()) {
		gl::ImageInfoData info = a->getImageInfo();
		auto &constraints = req->getFrameConstraints();
		auto screenSize = constraints.getScreenSize();
		info.extent =  Extent2(std::ceil((screenSize.width / constraints.density) * _shadowDensity),
				std::ceil((screenSize.height / constraints.density) * _shadowDensity));
		req->addImageSpecialization(a, move(info));

		if (_cachedShadowDensity != _shadowDensity / req->getFrameConstraints().density
				|| _cachedLightsCount != _lightsAmbientCount + _lightsDirectCount) {
			_cachedShadowDensity = _shadowDensity / req->getFrameConstraints().density;
			_cachedLightsCount = _lightsAmbientCount + _lightsDirectCount;
			req->setAttachmentsDirty(true);
		}
	}

	if (_cacheDirty) {
		req->setAttachmentsDirty(true);
	}

	req->setQueue(_queue);
}

void Scene::addChildNode(Node *child) {
	_content->addChildNode(child);
}

void Scene::addChildNode(Node *child, int16_t localZOrder) {
	_content->addChildNode(child, localZOrder);
}

void Scene::addChildNode(Node *child, int16_t localZOrder, uint64_t tag) {
	_content->addChildNode(child, localZOrder, tag);
}

const Size2& Scene::getContentSize() const {
	return _content->getContentSize();
}

bool Scene::addLight(SceneLight *light, uint64_t tag, StringView name) {
	if (tag != InvalidTag) {
		if (_lightsByTag.find(tag) != _lightsByTag.end()) {
			log::vtext("Scene", "Light with tag ", tag, " is already defined");
			return false;
		}
	}
	if (!name.empty()) {
		if (_lightsByName.find(name) != _lightsByName.end()) {
			log::vtext("Scene", "Light with name ", name, " is already defined");
			return false;
		}
	}

	if (light->getScene()) {
		log::vtext("Scene", "Light is already on scene");
		return false;
	}

	switch (light->getType()) {
	case SceneLightType::Ambient:
		if (_lightsAmbientCount >= config::MaxAmbientLights) {
			log::vtext("Scene", "Too many ambient lights");
			return false;
		}
		break;
	case SceneLightType::Direct:
		if (_lightsDirectCount >= config::MaxDirectLights) {
			log::vtext("Scene", "Too many direct lights");
			return false;
		}
		break;
	}

	_lights.emplace_back(light);

	switch (light->getType()) {
	case SceneLightType::Ambient: ++ _lightsAmbientCount; break;
	case SceneLightType::Direct: ++ _lightsDirectCount; break;
	}

	if (tag != InvalidTag) {
		light->setTag(tag);
		_lightsByTag.emplace(tag, light);
	}

	if (!name.empty()) {
		light->setName(name);
		_lightsByName.emplace(light->getName(), light);
	}

	if (_running) {
		light->onEnter(this);
	} else {
		light->_scene = this;
	}

	return true;
}

SceneLight *Scene::getLightByTag(uint64_t tag) const {
	auto it = _lightsByTag.find(tag);
	if (it != _lightsByTag.end()) {
		return it->second;
	}
	return nullptr;
}

SceneLight *Scene::getLightByName(StringView name) const {
	auto it = _lightsByName.find(name);
	if (it != _lightsByName.end()) {
		return it->second;
	}
	return nullptr;
}

void Scene::removeLight(SceneLight *light) {
	if (light->getScene() != this) {
		return;
	}

	auto it = _lights.begin();
	while (it != _lights.end()) {
		if (*it == light) {
			removeLight(it);
			return;
		}
		++ it;
	}
}

void Scene::removeLightByTag(uint64_t tag) {
	if (auto l = getLightByTag(tag)) {
		removeLight(l);
	}
}

void Scene::removeLightByName(StringView name) {
	if (auto l = getLightByName(name)) {
		removeLight(l);
	}
}

void Scene::removeAllLights() {
	auto it = _lights.begin();
	while (it != _lights.end()) {
		it = removeLight(it);
	}
}

void Scene::removeAllLightsByType(SceneLightType type) {
	auto it = _lights.begin();
	while (it != _lights.end()) {
		if ((*it)->getType() == type) {
			it = removeLight(it);
		} else {
			++ it;
		}
	}
}

void Scene::setGlobalLight(const Color4F &color) {
	_globalLight = color;
}

const Color4F & Scene::getGlobalLight() const {
	return _globalLight;
}

void Scene::setClipContent(bool value) {
	if (isClipContent() != value) {
		if (value) {
			_content->enableScissor();
		} else {
			_content->disableScissor();
		}
	}
}

bool Scene::isClipContent() const {
	return _content->isScissorEnabled();
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

		for (auto &it : subpasses) {
			// check if subpass has material attachment
			bool isUsable = false;
			for (auto &attachment : it.inputBuffers) {
				if (attachment->getAttachment() == a) {
					isUsable = true;
					break;
				}
			}

			if (!isUsable) {
				break;
			}

			auto &sp = v.subasses.emplace_back(SubpassData({&it}));

			for (auto &pipeline : it.graphicPipelines) {
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

Vector<Rc<SceneLight>>::iterator Scene::removeLight(Vector<Rc<SceneLight>>::iterator itVec) {
	if ((*itVec)->isRunning()) {
		(*itVec)->onExit();
	}

	if (!(*itVec)->getName().empty()) {
		auto it = _lightsByName.find((*itVec)->getName());
		if (it != _lightsByName.end()) {
			_lightsByName.erase(it);
		}
	}

	if ((*itVec)->getTag() != InvalidTag) {
		auto it = _lightsByTag.find((*itVec)->getTag());
		if (it != _lightsByTag.end()) {
			_lightsByTag.erase(it);
		}
	}

	switch ((*itVec)->getType()) {
	case SceneLightType::Ambient: -- _lightsAmbientCount; break;
	case SceneLightType::Direct: -- _lightsDirectCount; break;
	}

	return _lights.erase(itVec);
}

}
