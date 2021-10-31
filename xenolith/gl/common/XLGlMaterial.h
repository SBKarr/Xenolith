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

#ifndef XENOLITH_GL_COMMON_XLGLMATERIAL_H_
#define XENOLITH_GL_COMMON_XLGLMATERIAL_H_

#include "XLGlResource.h"
#include "XLGlAttachment.h"

namespace stappler::xenolith::gl {

class Material;
class MaterialSet;
class MaterialAttachment;

struct MaterialInputData : gl::AttachmentInputData {
	Rc<MaterialAttachment> attachment;
	Vector<Rc<Material>> materials; // new materials to update buffer
};

struct MaterialImage {
	const ImageData *image;
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
	using FinalizeCallback = Function<void(Rc<TextureSet> &&)>;

	virtual ~MaterialSet();

	bool init(const BufferInfo &, const EncodeCallback &callback, const FinalizeCallback &,
			uint32_t objectSize, uint32_t imagesInSet);
	bool init(const Rc<MaterialSet> &);

	bool encode(uint8_t *, const Material *);

	void clear();

	Vector<Material *> updateMaterials(const Rc<MaterialInputData> &,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);
	Vector<Material *> updateMaterials(const Vector<Rc<Material>> &materials,
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
	void emplaceMaterialImages(Material *oldMaterial, Material *newMaterial,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);

	BufferInfo _info;
	EncodeCallback _encodeCallback;
	FinalizeCallback _finalizeCallback;
	uint32_t _objectSize = 0;
	uint32_t _imagesInSet = 16;

	uint32_t _generation = 1;
	std::unordered_map<MaterialId, Rc<Material>> _materials;
	std::unordered_map<MaterialId, uint32_t> _ordering;

	// describes image location in descriptor sets
	// all images from same material must be in one set
	Vector<MaterialLayout> _layouts;

	Rc<BufferObject> _buffer;
	Rc<TextureSet> _textureSet;
};

class Material final : public Ref {
public:
	virtual ~Material();

	// view for image must be empty
	bool init(const PipelineData *, Vector<MaterialImage> &&, Bytes && = Bytes());
	bool init(const PipelineData *, const ImageData *, Bytes && = Bytes());

	MaterialId getId() const { return _id; }
	const PipelineData * getPipeline() const { return _pipeline; }
	const Vector<MaterialImage> &getImages() const { return _images; }
	BytesView getData() const { return _data; }

	uint32_t getLayoutIndex() const { return _layoutIndex; }
	void setLayoutIndex(uint32_t);

protected:
	friend class MaterialSet;

	bool _dirty = true;
	MaterialId _id = 0;
	uint32_t _layoutIndex = 0; // set after compilation
	const PipelineData *_pipeline;
	Vector<MaterialImage> _images;
	Bytes _data;
};

// this attachment should provide material data buffer for rendering
class MaterialAttachment : public BufferAttachment {
public:
	virtual ~MaterialAttachment();

	virtual bool init(StringView, const BufferInfo &, MaterialSet::EncodeCallback &&, MaterialSet::FinalizeCallback &&,
			uint32_t materialObjectSize, MaterialType type, Vector<Rc<Material>> &&);

	const Rc<gl::MaterialSet> &getMaterials() const;
	void setMaterials(const Rc<gl::MaterialSet> &);

	MaterialType getType() const { return _type; }

	const Vector<Rc<Material>> &getInitialMaterials() const { return _initialMaterials; }

	virtual Rc<gl::MaterialSet> allocateSet(const Device &) const;
	virtual Rc<gl::MaterialSet> cloneSet(const Rc<MaterialSet> &) const;

	virtual void sortDescriptors(RenderQueue &queue, Device &dev) override;

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;

	uint32_t _materialObjectSize = 0;
	MaterialType _type;
	MaterialSet::EncodeCallback _encodeCallback;
	MaterialSet::FinalizeCallback _finalizeCallback;
	Rc<gl::MaterialSet> _data;
	Vector<Rc<Material>> _initialMaterials;
};

class MaterialAttachmentDescriptor : public BufferAttachmentDescriptor {
public:
	virtual ~MaterialAttachmentDescriptor() { }

	virtual bool init(RenderPassData *, Attachment *);

	uint64_t getBoundGeneration() const { return _boundGeneration.load(); }
	void setBoundGeneration(uint64_t);

protected:
	std::atomic<uint64_t> _boundGeneration = 0;
};

class SamplersAttachment : public GenericAttachment {
public:
	virtual ~SamplersAttachment();

	virtual bool init(StringView);

protected:
	virtual Rc<AttachmentDescriptor> makeDescriptor(RenderPassData *) override;
};

class SamplersAttachmentDescriptor : public GenericAttachmentDescriptor {
public:
	virtual ~SamplersAttachmentDescriptor();

	virtual bool init(RenderPassData *, Attachment *) override;
	virtual void sortRefs(RenderQueue &, Device &dev) override;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLMATERIAL_H_ */

