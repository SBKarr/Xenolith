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

#ifndef COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_
#define COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_

#include "XLNode.h"
#include "XLRenderQueueResource.h"
#include "XLRenderQueueQueue.h"
#include "XLGlMaterial.h"
#include "XLSceneLight.h"
#include "XLDynamicStateNode.h"

namespace stappler::xenolith {

class SceneLight;

class Scene : public Node {
public:
	using RenderQueue = renderqueue::Queue;
	using FrameRequest = renderqueue::FrameRequest;
	using FrameQueue = renderqueue::FrameQueue;
	using FrameHandle = renderqueue::FrameHandle;
	using PipelineData = renderqueue::GraphicPipelineData;
	using PipelineInfo = renderqueue::GraphicPipelineInfo;

	virtual ~Scene();

	virtual bool init(Application *, RenderQueue::Builder &&, const gl::FrameContraints &);

	virtual void renderRequest(const Rc<FrameRequest> &);
	virtual void render(RenderFrameInfo &info);

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void onContentSizeDirty() override;

	const Rc<RenderQueue> &getRenderQueue() const { return _queue; }
	Director *getDirector() const { return _director; }

	virtual void onPresented(Director *);
	virtual void onFinished(Director *);

	virtual void onFrameStarted(FrameRequest &);
	virtual void onFrameEnded(FrameRequest &);

	virtual uint64_t getMaterial(const MaterialInfo &) const;

	// dynamically load material
	// this can be severe less effective then pre-initialized materials,
	// so, it's preferred to pre-initialize all materials in release builds
	// virtual uint64_t acquireMaterial(const MaterialInfo &, const Vector<const gl::ImageData *> &images);
	virtual uint64_t acquireMaterial(const MaterialInfo &, Vector<gl::MaterialImage> &&images, bool revokable);

	virtual void setFrameConstraints(const gl::FrameContraints &);
	const gl::FrameContraints & getFrameConstraints() const { return _constraints; }

	virtual void revokeImages(const Vector<uint64_t> &);

	virtual void specializeRequest(const Rc<FrameRequest> &req);

	virtual void addChildNode(Node *child) override;
	virtual void addChildNode(Node *child, int16_t localZOrder) override;
	virtual void addChildNode(Node *child, int16_t localZOrder, uint64_t tag) override;

	virtual const Size2& getContentSize() const override;

	virtual bool addLight(SceneLight *, uint64_t tag = InvalidTag, StringView name = StringView());

	virtual SceneLight *getLightByTag(uint64_t) const;
	virtual SceneLight *getLightByName(StringView) const;

	virtual void removeLight(SceneLight *);
	virtual void removeLightByTag(uint64_t);
	virtual void removeLightByName(StringView);

	virtual void removeAllLights();
	virtual void removeAllLightsByType(SceneLightType);

	virtual void setGlobalLight(const Color4F &);
	virtual const Color4F & getGlobalLight() const;

	virtual void setClipContent(bool);
	virtual bool isClipContent() const;

protected:
	using Node::init;

	struct SubpassData {
		renderqueue::SubpassData *subpass;
		std::unordered_map<size_t, Vector<const PipelineData *>> pipelines;
	};

	struct AttachmentData {
		const gl::MaterialAttachment *attachment;
		Vector<SubpassData> subasses;
	};

	virtual Rc<RenderQueue> makeQueue(RenderQueue::Builder &&);
	virtual void readInitialMaterials(gl::MaterialAttachment *);
	virtual MaterialInfo getMaterialInfo(gl::MaterialType, const Rc<gl::Material> &) const;

	virtual gl::ImageViewInfo getImageViewForMaterial(const MaterialInfo &, uint32_t idx, const gl::ImageData *) const;
	virtual Bytes getDataForMaterial(const gl::MaterialAttachment *, const MaterialInfo &) const;

	// Search for pipeline, compatible with material
	// Backward-search through RenderPass/Subppass, that uses material attachment provided
	// Can be slow on complex RenderQueue
	virtual const PipelineData *getPipelineForMaterial(const AttachmentData &, const MaterialInfo &) const;
	virtual bool isPipelineMatch(const PipelineInfo *, const MaterialInfo &) const;

	void addPendingMaterial(const gl::MaterialAttachment *, Rc<gl::Material> &&);
	void addMaterial(const MaterialInfo &, gl::MaterialId, bool revokable);

	void listMaterials() const;

	Vector<Rc<SceneLight>>::iterator removeLight(Vector<Rc<SceneLight>>::iterator);

	struct SceneMaterialInfo {
		MaterialInfo info;
		gl::MaterialId id;
		bool revokable;
	};

	struct PendingData {
		Vector<Rc<gl::Material>> toAdd;
		Vector<uint32_t> toRemove;
	};

	Application *_application = nullptr;
	Director *_director = nullptr;
	DynamicStateNode *_content = nullptr;

	Rc<RenderQueue> _queue;

	Map<gl::MaterialType, AttachmentData> _attachmentsByType;
	std::unordered_map<uint64_t, Vector<SceneMaterialInfo>> _materials;

	Map<const gl::MaterialAttachment *, PendingData> _pending;
	Rc<renderqueue::DependencyEvent> _materialDependency;

	renderqueue::Attachment *_bufferAttachment = nullptr;

	// отозванные ид могут быть выданы новым отзываемым материалам, чтобы не засорять биндинги
	Vector<gl::MaterialId> _revokedIds;

	float _shadowDensity = 0.5f;

	uint32_t _lightsAmbientCount = 0;
	uint32_t _lightsDirectCount = 0;
	Vector<Rc<SceneLight>> _lights;
	Map<uint64_t, SceneLight *> _lightsByTag;
	Map<StringView, SceneLight *> _lightsByName;

	Color4F _globalLight = Color4F(1.0f, 1.0f, 1.0f, 1.0f);
	gl::FrameContraints _constraints;

	bool _cacheDirty = false;
	float _cachedShadowDensity = nan();
	uint32_t _cachedLightsCount = 0;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_ */
