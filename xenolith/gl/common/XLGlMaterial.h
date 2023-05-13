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

#ifndef XENOLITH_GL_COMMON_XLGLMATERIAL_H_
#define XENOLITH_GL_COMMON_XLGLMATERIAL_H_

#include "XLRenderQueueResource.h"
#include "XLRenderQueueAttachment.h"
#include "XLGlDynamicImage.h"

namespace stappler::xenolith::gl {

class Loop;
class Material;
class MaterialSet;
class MaterialAttachment;

using AttachmentInputData = renderqueue::AttachmentInputData;

struct MaterialInputData : AttachmentInputData {
	const MaterialAttachment * attachment;
	Vector<Rc<Material>> materialsToAddOrUpdate;
	Vector<MaterialId> materialsToRemove;
	Vector<MaterialId> dynamicMaterialsToUpdate;
};

struct MaterialImage {
	const ImageData *image;
	Rc<DynamicImageInstance> dynamic;
	ImageViewInfo info;
	Rc<ImageView> view;
	uint16_t sampler = 0;
	uint32_t set;
	uint32_t descriptor;

	bool canAlias(const MaterialImage &) const;
};

class MaterialSet final : public Ref {
public:
	using ImageSlot = MaterialImageSlot;
	using EncodeCallback = Function<bool(uint8_t *, const Material *)>;

	virtual ~MaterialSet();

	bool init(const BufferInfo &, const EncodeCallback &callback,
			uint32_t objectSize, uint32_t imagesInSet, uint32_t buffersInSet, const MaterialAttachment * = nullptr);
	bool init(const Rc<MaterialSet> &);

	bool encode(uint8_t *, const Material *);

	void clear();

	Vector<Rc<Material>> updateMaterials(const Rc<MaterialInputData> &,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);
	Vector<Rc<Material>> updateMaterials(const Vector<Rc<Material>> &materials, SpanView<MaterialId> dynamic, SpanView<MaterialId> remove,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);

	const BufferInfo &getInfo() const { return _info; }
	uint32_t getObjectSize() const { return _objectSize; }
	uint32_t getImagesInSet() const { return _imagesInSet; }
	uint64_t getGeneration() const { return _generation; }
	const std::unordered_map<MaterialId, Rc<Material>> &getMaterials() const { return _materials; }

	void setBuffer(Rc<BufferObject> &&, std::unordered_map<MaterialId, uint32_t> &&);
	Rc<BufferObject> getBuffer() const { return _buffer; }
	const std::unordered_map<MaterialId, uint32_t> & getOrdering() const { return _ordering; }

	Vector<MaterialLayout> &getLayouts() { return _layouts; }
	const MaterialLayout *getLayout(uint32_t) const;
	const Material * getMaterialById(MaterialId) const;
	uint32_t getMaterialOrder(MaterialId) const;

protected:
	void removeMaterial(Material *oldMaterial);
	void emplaceMaterialImages(Material *oldMaterial, Material *newMaterial,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);

	BufferInfo _info;
	EncodeCallback _encodeCallback;
	uint32_t _objectSize = 0;
	uint32_t _imagesInSet = 16;
	uint32_t _buffersInSet = 16;

	uint32_t _generation = 1;
	std::unordered_map<MaterialId, Rc<Material>> _materials;
	std::unordered_map<MaterialId, uint32_t> _ordering;

	// describes image location in descriptor sets
	// all images from same material must be in one set
	Vector<MaterialLayout> _layouts;

	Rc<BufferObject> _buffer;
	const MaterialAttachment *_owner = nullptr;
};

class Material final : public Ref {
public:
	// Использовать только для определения встроенных в аттачмент материалов
	static constexpr auto MaterialIdInitial = maxOf<uint32_t>();

	using PipelineData = renderqueue::GraphicPipelineData;

	virtual ~Material();

	// view for image must be empty
	bool init(MaterialId, const PipelineData *, Vector<MaterialImage> &&, Bytes && = Bytes());
	bool init(MaterialId, const PipelineData *, const Rc<DynamicImageInstance> &, Bytes && = Bytes());
	bool init(MaterialId, const PipelineData *, const ImageData *, Bytes && = Bytes(), bool ownedData = false);
	bool init(MaterialId, const PipelineData *, const ImageData *, ColorMode, Bytes && = Bytes(), bool ownedData = false);
	bool init(const Material *, Rc<ImageObject> &&, Rc<DataAtlas> &&, Bytes && = Bytes());
	bool init(const Material *, Vector<MaterialImage> &&);

	MaterialId getId() const { return _id; }
	const PipelineData * getPipeline() const { return _pipeline; }
	const Vector<MaterialImage> &getImages() const { return _images; }
	BytesView getData() const { return _data; }

	uint32_t getLayoutIndex() const { return _layoutIndex; }
	void setLayoutIndex(uint32_t);

	const Rc<DataAtlas> &getAtlas() const { return _atlas; }
	const ImageData *getOwnedData() const { return _ownedData; }

protected:
	friend class MaterialSet;
	friend class MaterialAttachment; // for id replacement

	bool _dirty = true;
	MaterialId _id = 0;
	uint32_t _layoutIndex = 0; // set after compilation
	const PipelineData *_pipeline;
	Vector<MaterialImage> _images;
	Rc<DataAtlas> _atlas;
	Bytes _data;
	const ImageData *_ownedData = nullptr;
};

// this attachment should provide material data buffer for rendering
class MaterialAttachment : public renderqueue::BufferAttachment {
public:
	virtual ~MaterialAttachment();

	virtual bool init(AttachmentBuilder &builder, const BufferInfo &, MaterialSet::EncodeCallback &&,
			uint32_t materialObjectSize, MaterialType type);

	void addPredefinedMaterials(Vector<Rc<Material>> &&);

	const Rc<gl::MaterialSet> &getMaterials() const;
	void setMaterials(const Rc<gl::MaterialSet> &) const;

	MaterialType getType() const { return _type; }

	const Vector<Rc<Material>> &getPredefinedMaterials() const { return _predefinedMaterials; }

	virtual Rc<gl::MaterialSet> allocateSet(const Device &) const;
	virtual Rc<gl::MaterialSet> cloneSet(const Rc<MaterialSet> &) const;

	virtual void sortDescriptors(RenderQueue &queue, Device &dev) override;

	virtual void addDynamicTracker(MaterialId, const Rc<DynamicImage> &) const;
	virtual void removeDynamicTracker(MaterialId, const Rc<DynamicImage> &) const;

	virtual void updateDynamicImage(Loop &, const DynamicImage *,
			const Vector<Rc<renderqueue::DependencyEvent>> & = Vector<Rc<renderqueue::DependencyEvent>>()) const;

	MaterialId getNextMaterialId() const;

protected:
	using BufferAttachment::init;

	struct DynamicImageTracker {
		uint32_t refCount;
		Map<MaterialId, uint32_t> materials;
	};

	mutable std::atomic<MaterialId> _attachmentMaterialId = 1;
	uint32_t _materialObjectSize = 0;
	MaterialType _type;
	MaterialSet::EncodeCallback _encodeCallback;
	mutable Rc<gl::MaterialSet> _data;
	Vector<Rc<Material>> _predefinedMaterials;

	mutable Mutex _dynamicMutex;
	mutable Map<Rc<DynamicImage>, DynamicImageTracker> _dynamicTrackers;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLMATERIAL_H_ */

