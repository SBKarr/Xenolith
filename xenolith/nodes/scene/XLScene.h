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

#ifndef COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_
#define COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_

#include "XLNode.h"
#include "XLGlResource.h"
#include "XLGlRenderQueue.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith {

class Scene : public Node {
public:
	virtual ~Scene();

	virtual bool init(gl::RenderQueue::Builder &&);
	virtual bool init(gl::RenderQueue::Builder &&, Size);

	virtual void render(RenderFrameInfo &info);

	virtual void onContentSizeDirty() override;

	const Rc<gl::RenderQueue> &getRenderQueue() const { return _queue; }
	Director *getDirector() const { return _director; }

	virtual void onPresented(Director *);
	virtual void onFinished(Director *);

	virtual void onFrameStarted(gl::FrameHandle &); // called on GL thread;
	virtual void onFrameEnded(gl::FrameHandle &); // called on GL thread;
	virtual void on2dVertexInput(gl::FrameHandle &, const Rc<gl::AttachmentHandle> &); // called on GL thread;

	virtual void onQueueEnabled(const gl::Swapchain *);
	virtual void onQueueDisabled();

	virtual uint64_t getMaterial(const MaterialInfo &) const;

	// dynamically load material
	// this can be severe less effective then pre-initialized materials,
	// so, it's preferred to pre-initialize all materials in release builds
	virtual uint64_t acquireMaterial(const MaterialInfo &, const Vector<const gl::ImageData *> &images);

protected:
	virtual Rc<gl::RenderQueue> makeQueue(gl::RenderQueue::Builder &&);
	virtual void readInitialMaterials();
	virtual MaterialInfo getMaterialInfo(gl::MaterialType, const Rc<gl::Material> &) const;

	virtual gl::ImageViewInfo getImageViewForMaterial(const MaterialInfo &, uint32_t idx, const gl::ImageData *) const;
	virtual Bytes getDataForMaterial(const gl::MaterialAttachment *, const MaterialInfo &) const;

	// Search for pipeline, compatible with material
	// Backward-search through RenderPass/Subppass, that uses material attachment provided
	// Can be slow on complex RenderQueue
	virtual const gl::PipelineData *getPipelineForMaterial(const gl::MaterialAttachment *, const MaterialInfo &) const;
	virtual bool isPipelineMatch(const gl::PipelineData *, const MaterialInfo &) const;

	const gl::MaterialAttachment *getAttachmentByType(gl::MaterialType) const;

	void addPendingMaterial(const gl::MaterialAttachment *, Rc<gl::Material> &&);
	void addMaterial(const MaterialInfo &, gl::MaterialId);

	uint32_t _refId = 0;
	Director *_director = nullptr;
	Rc<gl::RenderQueue> _queue;

	Map<gl::MaterialType, const gl::MaterialAttachment *> _attachmentsByType;
	std::unordered_map<uint64_t, Vector<Pair<MaterialInfo, gl::MaterialId>>> _materials;

	Map<const gl::MaterialAttachment *, Vector<Rc<gl::Material>>> _pendingMaterials;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_SCENE_XLSCENE_H_ */
