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

#include "XLGlMaterial.h"
#include "XLGlRenderPass.h"

namespace stappler::xenolith::gl {

MaterialSet::~MaterialSet() { }

bool MaterialSet::init(const BufferInfo &info, const EncodeCallback &callback, uint32_t objectSize, uint32_t imagesInSet) {
	_info = info;
	_encodeCallback = callback;
	_objectSize = objectSize;
	_imagesInSet = imagesInSet;
	_info.size = 0;
	return true;
}

bool MaterialSet::init(const Rc<MaterialSet> &other) {
	_info = other->_info;
	_encodeCallback = other->_encodeCallback;
	_generation = other->_generation + 1;
	_materials = other->_materials;
	_objectSize = other->_objectSize;
	_imagesInSet = other->_imagesInSet;
	_layouts = other->_layouts;

	for (auto &it : _layouts) {
		it.set = nullptr;
	}

	return true;
}

bool MaterialSet::encode(uint8_t *buf, const Material *material) {
	if (_encodeCallback) {
		return _encodeCallback(buf, material);
	}
	return false;
}

Vector<Material *> MaterialSet::updateMaterials(const Rc<MaterialInputData> &data,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	return updateMaterials(data->materials, cb);
}

Vector<Material *> MaterialSet::updateMaterials(const Vector<Rc<Material>> &materials,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	Vector<Material *> ret; ret.reserve(materials.size());
	for (auto &material : materials) {
		auto mIt = _materials.find(material->getId());
		if (mIt != _materials.end()) {
			emplaceMaterialImages(mIt->second, material.get(), cb);
			mIt->second = move(material);
			ret.emplace_back(mIt->second.get());
		} else {
			auto it = _materials.emplace(material->getId(), move(material)).first;
			emplaceMaterialImages(nullptr, it->second.get(), cb);
			ret.emplace_back(it->second.get());
		}
	}
	_info.size = _objectSize * _materials.size();
	return ret;
}

void MaterialSet::setBuffer(Rc<BufferObject> &&buffer) {
	_buffer = move(buffer);
}

const MaterialLayout *MaterialSet::getLayout(uint32_t idx) const {
	if (idx < _layouts.size()) {
		return &_layouts[idx];
	}
	return nullptr;
}

const Material * MaterialSet::getMaterial(uint32_t idx) const {
	auto it = _materials.find(idx);
	if (it != _materials.end()) {
		return it->second.get();
	}
	return nullptr;
}

void MaterialSet::emplaceMaterialImages(Material *oldMaterial, Material *newMaterial,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	Vector<MaterialImage> *oldImages = nullptr;
	uint32_t targetSet = maxOf<uint32_t>();
	if (oldMaterial) {
		targetSet = oldMaterial->getLayoutIndex();
		oldImages = &oldMaterial->_images;
	}

	auto &newImages = newMaterial->_images;
	if (oldImages) {
		auto &oldSet = _layouts[targetSet];
		// remove non-aliased images
		for (auto &oIt : *oldImages) {
			bool hasAlias = false;
			for (auto &nIt : newImages) {
				if (oIt.canAlias(nIt)) {
					hasAlias = true;
					break;
				}
			}
			if (!hasAlias) {
				-- oldSet.slots[oIt.descriptor].refCount;
				if (oldSet.slots[oIt.descriptor].refCount == 0) {
					oldSet.slots[oIt.descriptor].image = nullptr;
				}
				oIt.view = nullptr;
			}
		}
	}

	Vector<Pair<const MaterialImage *, Vector<uint32_t>>> uniqueImages;

	// find unique images
	uint32_t imageIdx = 0;
	for (auto &it : newImages) {
		it.info = it.image->getViewInfo(it.info);

		bool isAlias = false;
		for (auto &uit : uniqueImages) {
			if (uit.first->canAlias(it)) {
				uit.second.emplace_back(imageIdx);
				isAlias = true;
			}
		}
		if (!isAlias) {
			uniqueImages.emplace_back(pair(&it, Vector<uint32_t>({imageIdx})));
		}
		++ imageIdx;
	}

	auto emplaceMaterial = [&] (uint32_t setIdx, MaterialLayout &set, Vector<uint32_t> &locations) {
		if (locations.empty()) {
			for (uint32_t imageIdx = 0; imageIdx < uniqueImages.size(); ++ imageIdx) {
				locations.emplace_back(imageIdx);
			}
		}

		uint32_t imageIdx = 0;
		for (auto &it : uniqueImages) {
			auto loc = locations[imageIdx];
			if (set.slots[loc].image) {
				// increment slot refcount, if image already exists
				set.slots[loc].refCount += it.second.size();
			} else {
				// fill slot with new ImageView
				set.slots[loc].image = cb(*it.first);
				set.slots[loc].image->setLocation(setIdx, loc);
				set.slots[loc].refCount = it.second.size();
				set.usedSlots = std::max(set.usedSlots, loc + 1);
			}

			// fill refs
			for (auto &iIt : it.second) {
				newImages[iIt].view = set.slots[loc].image;
				newImages[iIt].set = setIdx;
				newImages[iIt].descriptor = loc;
			}

			++ imageIdx;
		}

		newMaterial->setLayoutIndex(setIdx);

		if (oldImages) {
			auto &oldSet = _layouts[targetSet];
			// remove non-aliased images
			for (auto &oIt : *oldImages) {
				if (oIt.view) {
					-- oldSet.slots[oIt.descriptor].refCount;
					if (oldSet.slots[oIt.descriptor].refCount == 0) {
						oldSet.slots[oIt.descriptor].image = nullptr;
					}
					oIt.view = nullptr;
				}
			}
		}
	};

	auto tryToEmplaceSet = [&] (uint32_t setIndex, MaterialLayout &set) -> bool {
		uint32_t emplaced = 0;
		Vector<uint32_t> positions; positions.resize(uniqueImages.size(), maxOf<uint32_t>());

		imageIdx = 0;
		// for each unique image, find it's potential place in set
		for (auto &uit : uniqueImages) {
			uint32_t location = 0;
			for (auto &it : set.slots) {
				// check if image can alias with existed
				if (it.image && it.image->getImage() == uit.first->image->image && it.image->getInfo() == uit.first->info) {
					if (positions[imageIdx] == maxOf<uint32_t>()) {
						++ emplaced; // mark as emplaced only if not emplaced already
					}
					positions[imageIdx] = location;
					break; // stop searching - best variant
				} else if (!it.image || it.refCount == 0) {
					// only if not emplaced
					if (positions[imageIdx] == maxOf<uint32_t>()) {
						++ emplaced;
						positions[imageIdx] = location;
					}
					// continue searching for possible alias
				}
				++ location;
			}

			++ imageIdx;
		}

		// if all images emplaced, perform actual emplace and return
		if (emplaced == uniqueImages.size()) {
			emplaceMaterial(setIndex, set, positions);
			return true;
		}
		return false;
	};

	if (targetSet != maxOf<uint32_t>()) {
		if (tryToEmplaceSet(targetSet, _layouts[targetSet])) {
			return;
		}
	}

	// process existed sets
	uint32_t setIndex = 0;
	for (auto &set : _layouts) {
		if (setIndex == targetSet) {
			continue;
		}

		if (tryToEmplaceSet(setIndex, set)) {
			return;
		}

		// or continue to search for appropriate set
		++ setIndex;
	}

	// no available set, create new one;
	auto &nIt = _layouts.emplace_back(MaterialLayout());
	nIt.slots.resize(_imagesInSet);

	Vector<uint32_t> locations;
	emplaceMaterial(_layouts.size() - 1, nIt, locations);
}

bool MaterialImage::canAlias(const MaterialImage &other) const {
	return other.image == image && other.info == info;
}

Material::~Material() { }

bool Material::init(uint32_t id, const PipelineData *pipeline, Vector<MaterialImage> &&images, Bytes &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = move(images);
	_data = move(data);
	return true;
}

bool Material::init(uint32_t id, const PipelineData *pipeline, const ImageData *image, Bytes &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = Vector<MaterialImage>({
		MaterialImage({
			image
		})
	});
	_data = move(data);
	return true;
}

void Material::setLayoutIndex(uint32_t idx) {
	_layoutIndex = idx;
}

MaterialAttachment::~MaterialAttachment() { }

bool MaterialAttachment::init(StringView name, const BufferInfo &info,
		MaterialSet::EncodeCallback &&cb, uint32_t size, Vector<Rc<Material>> &&initials) {
	if (BufferAttachment::init(name, info)) {
		_materialObjectSize = size;
		_encodeCallback = move(cb);
		_initialMaterials = move(initials);
		return true;
	}
	return false;
}

const Rc<gl::MaterialSet> &MaterialAttachment::getMaterials() const {
	return _data;
}

void MaterialAttachment::setMaterials(const Rc<gl::MaterialSet> &data) {
	_data = data;
}

Rc<gl::MaterialSet> MaterialAttachment::allocateSet(const Device &dev) const {
	return Rc<gl::MaterialSet>::create(_info, _encodeCallback, _materialObjectSize, dev.getTextureLayoutImagesCount());
}

Rc<gl::MaterialSet> MaterialAttachment::cloneSet(const Rc<gl::MaterialSet> &other) const {
	return Rc<gl::MaterialSet>::create(other);
}

void MaterialAttachment::sortDescriptors(RenderQueue &queue, Device &dev) {
	BufferAttachment::sortDescriptors(queue, dev);
	if (!_data) {
		_data = allocateSet(dev);
	}
}

Rc<AttachmentDescriptor> MaterialAttachment::makeDescriptor(RenderPassData *pass) {
	return Rc<MaterialAttachmentDescriptor>::create(pass, this);
}

bool MaterialAttachmentDescriptor::init(RenderPassData *data, Attachment *attachment) {
	if (BufferAttachmentDescriptor::init(data, attachment)) {
		_usesTextureSet = true;
		return true;
	}
	return false;
}

void MaterialAttachmentDescriptor::setBoundGeneration(uint64_t gen) {
	_boundGeneration = gen;
}

SamplersAttachment::~SamplersAttachment() { }

bool SamplersAttachment::init(StringView name) {
	if (GenericAttachment::init(name)) {
		_descriptorType = DescriptorType::Sampler;
		return true;
	}
	return false;
}

Rc<AttachmentDescriptor> SamplersAttachment::makeDescriptor(RenderPassData *data) {
	return Rc<SamplersAttachmentDescriptor>::create(data, this);
}

SamplersAttachmentDescriptor::~SamplersAttachmentDescriptor() { }

bool SamplersAttachmentDescriptor::init(RenderPassData *data, Attachment *a) {
	if (GenericAttachmentDescriptor::init(data, a)) {
		_descriptor.type = DescriptorType::Sampler;
		return true;
	}
	return false;
}

void SamplersAttachmentDescriptor::sortRefs(RenderQueue &queue, Device &dev) {
	GenericAttachmentDescriptor::sortRefs(queue, dev);

	_descriptor.maxCount = _descriptor.count = dev.getSamplersCount();
}

}
