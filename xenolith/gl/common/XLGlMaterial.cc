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

#include "XLGlMaterial.h"
#include "XLGlDynamicImage.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::gl {

MaterialSet::~MaterialSet() {
	clear();
}

bool MaterialSet::init(const BufferInfo &info, const EncodeCallback &callback,
		uint32_t objectSize, uint32_t imagesInSet, uint32_t buffersInSet, const MaterialAttachment *owner) {
	_info = info;
	_encodeCallback = callback;
	_objectSize = objectSize;
	_imagesInSet = imagesInSet;
	_buffersInSet = buffersInSet;
	_info.size = 0;
	_owner = owner;
	return true;
}

bool MaterialSet::init(const Rc<MaterialSet> &other) {
	_info = other->_info;
	_encodeCallback = other->_encodeCallback;
	_generation = other->_generation + 1;
	_materials = other->_materials;
	_objectSize = other->_objectSize;
	_imagesInSet = other->_imagesInSet;
	_buffersInSet = other->_buffersInSet;
	_layouts = other->_layouts;
	_owner = other->_owner;
	_buffer = other->_buffer;

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

void MaterialSet::clear() { }

Vector<Rc<Material>> MaterialSet::updateMaterials(const Rc<MaterialInputData> &data,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	return updateMaterials(data->materialsToAddOrUpdate, data->dynamicMaterialsToUpdate, data->materialsToRemove, cb);
}

Vector<Rc<Material>> MaterialSet::updateMaterials(const Vector<Rc<Material>> &materials, SpanView<MaterialId> dynamicMaterials,
		SpanView<MaterialId> materialsToRemove, const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	Vector<MaterialId> updatedIds; updatedIds.reserve(materials.size() + dynamicMaterials.size());
	Vector<Rc<Material>> ret; ret.reserve(materials.size());

	for (auto &it : materialsToRemove) {
		auto mIt = _materials.find(it);
		if (mIt != _materials.end()) {
			ret.emplace_back(mIt->second);
			removeMaterial(mIt->second);
			for (auto &it : mIt->second->getImages()) {
				if (it.dynamic && _owner) {
					_owner->removeDynamicTracker(mIt->second->getId(), it.dynamic->image);
				}
			}
			_materials.erase(mIt);
		}
	}

	for (auto &material : materials) {
		bool isImagesValid = true;

		if (!materialsToRemove.empty()) {
			auto it = std::find(materialsToRemove.begin(), materialsToRemove.end(), material->getId());
			if (it != materialsToRemove.end()) {
				continue;
			}
		}

		for (auto &it : material->_images) {
			if (!it.image) {
				isImagesValid = false;
			}
			if (it.dynamic) {
				// try to actualize image
				auto current = it.dynamic->image->getInstance();
				if (current != it.dynamic) {
					if (material->_atlas == it.image->atlas) {
						material->_atlas = current->data.atlas;
					}
					it.dynamic = current;
					it.image = &it.dynamic->data;
				}
				if (_owner) {
					_owner->addDynamicTracker(material->getId(), it.dynamic->image);
				}
			}
		}

		if (!isImagesValid) {
			continue;
		}

		updatedIds.emplace_back(material->getId());

		auto mIt = _materials.find(material->getId());
		if (mIt != _materials.end()) {
			emplaceMaterialImages(mIt->second, material.get(), cb);
			mIt->second = move(material);
			ret.emplace_back(mIt->second.get());
			for (auto &it : mIt->second->getImages()) {
				if (it.dynamic && _owner) {
					_owner->removeDynamicTracker(material->getId(), it.dynamic->image);
				}
			}
		} else {
			auto it = _materials.emplace(material->getId(), move(material)).first;
			emplaceMaterialImages(nullptr, it->second.get(), cb);
			ret.emplace_back(it->second.get());
		}
	}

	for (auto &it : dynamicMaterials) {
		if (!materialsToRemove.empty()) {
			auto iit = std::find(materialsToRemove.begin(), materialsToRemove.end(), it);
			if (iit != materialsToRemove.end()) {
				continue;
			}
		}

		auto mIt = _materials.find(it);
		if (mIt != _materials.end()) {
			auto &material = mIt->second;
			bool hasUpdates = false;
			Vector<Rc<DynamicImageInstance>> dynamics;
			dynamics.reserve(mIt->second->getImages().size());

			for (auto &image : mIt->second->getImages()) {
				if (image.dynamic) {
					auto current = image.dynamic->image->getInstance();
					if (current != image.dynamic) {
						hasUpdates = true;
						dynamics.emplace_back(current);
					} else {
						dynamics.emplace_back(nullptr);
					}
				} else {
					dynamics.emplace_back(nullptr);
				}
			}

			if (hasUpdates) {
				// create new material
				Vector<MaterialImage> images = material->getImages();
				size_t i = 0;
				for (auto &it : images) {
					if (auto v = dynamics[i]) {
						it.dynamic = v;
						it.image = &v->data;
					}
					it.view = nullptr;
					++ i;
				}

				auto mat = Rc<Material>::create(material, move(images));

				for (auto &it : mat->getImages()) {
					if (it.dynamic && _owner) {
						_owner->addDynamicTracker(mat->getId(), it.dynamic->image);
					}
				}

				emplaceMaterialImages(mIt->second, mat.get(), cb);
				mIt->second = move(mat);
				ret.emplace_back(mIt->second.get());
				for (auto &it : mIt->second->getImages()) {
					if (it.dynamic && _owner) {
						_owner->removeDynamicTracker(material->getId(), it.dynamic->image);
					}
				}
			}
		}
	}

	_info.size = _objectSize * _materials.size();

	if (_info.size == 0 || ret.size() == 0) {
		return Vector<Rc<Material>>();
	}
	return ret;
}

void MaterialSet::setBuffer(Rc<BufferObject> &&buffer, std::unordered_map<MaterialId, uint32_t> &&ordering) {
	_buffer = move(buffer);
	_ordering = move(ordering);
}

const MaterialLayout *MaterialSet::getLayout(uint32_t idx) const {
	if (idx < _layouts.size()) {
		return &_layouts[idx];
	}
	return nullptr;
}

const Material * MaterialSet::getMaterialById(MaterialId idx) const {
	auto it = _materials.find(idx);
	if (it != _materials.end()) {
		return it->second.get();
	}
	return nullptr;
}

uint32_t MaterialSet::getMaterialOrder(MaterialId idx) const {
	auto it = _ordering.find(idx);
	if (it != _ordering.end()) {
		return it->second;
	}
	return maxOf<uint32_t>();
}

void MaterialSet::removeMaterial(Material *oldMaterial) {
	auto &oldSet = _layouts[oldMaterial->getLayoutIndex()];
	for (auto &oIt : oldMaterial->_images) {
		-- oldSet.imageSlots[oIt.descriptor].refCount;
		if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
			oldSet.imageSlots[oIt.descriptor].image = nullptr;
		}
		oIt.view = nullptr;
	}
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
				-- oldSet.imageSlots[oIt.descriptor].refCount;
				if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
					oldSet.imageSlots[oIt.descriptor].image = nullptr;
				}
				oIt.view = nullptr;
			}
			if (oIt.image->atlas) {
				if (auto &atlasIndex = oIt.image->atlas->getIndexBuffer()) {
					auto descIdx = atlasIndex->getDescriptor();
					if (descIdx < oldSet.bufferSlots.size()) {
						-- oldSet.bufferSlots[descIdx].refCount;
						if (oldSet.bufferSlots[descIdx].refCount == 0) {
							oldSet.bufferSlots[descIdx].buffer = nullptr;
						}
					}
				}
				if (auto &atlasData = oIt.image->atlas->getDataBuffer()) {
					auto descIdx = atlasData->getDescriptor();
					if (descIdx < oldSet.bufferSlots.size()) {
						-- oldSet.bufferSlots[descIdx].refCount;
						if (oldSet.bufferSlots[descIdx].refCount == 0) {
							oldSet.bufferSlots[descIdx].buffer = nullptr;
						}
					}
				}
			}
		}
	}

	Vector<Pair<const MaterialImage *, Vector<uint32_t>>> uniqueImages;
	Vector<BufferObject *> uniqueBuffers;

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

		if (it.image->atlas) {
			if (auto &atlasIndex = it.image->atlas->getIndexBuffer()) {
				uniqueBuffers.emplace_back(atlasIndex);
			}
			if (auto &atlasData = it.image->atlas->getDataBuffer()) {
				uniqueBuffers.emplace_back(atlasData);
			}
		}
	}

	auto emplaceMaterial = [&] (uint32_t setIdx, MaterialLayout &set, Vector<uint32_t> &imageLocations, Vector<uint32_t> &bufferLocations) {
		if (imageLocations.empty()) {
			for (uint32_t imageIdx = 0; imageIdx < uniqueImages.size(); ++ imageIdx) {
				imageLocations.emplace_back(imageIdx);
			}
		}

		if (!uniqueBuffers.empty() && bufferLocations.empty()) {
			for (uint32_t bufferIdx = 0; bufferIdx < uniqueBuffers.size(); ++ bufferIdx) {
				bufferLocations.emplace_back(bufferIdx);
			}
		}

		uint32_t imageIdx = 0;
		for (auto &it : uniqueImages) {
			auto loc = imageLocations[imageIdx];
			if (set.imageSlots[loc].image) {
				// increment slot refcount, if image already exists
				set.imageSlots[loc].refCount += it.second.size();
			} else {
				// fill slot with new ImageView
				set.imageSlots[loc].image = cb(*it.first);
				set.imageSlots[loc].image->setLocation(setIdx, loc);
				set.imageSlots[loc].refCount = it.second.size();
				set.usedImageSlots = std::max(set.usedImageSlots, loc + 1);
			}

			// fill refs
			for (auto &iIt : it.second) {
				newImages[iIt].view = set.imageSlots[loc].image;
				newImages[iIt].set = setIdx;
				newImages[iIt].descriptor = loc;
			}

			++ imageIdx;
		}

		uint32_t bufferIdx = 0;
		for (auto &it : uniqueBuffers) {
			auto loc = bufferLocations[bufferIdx];
			if (set.bufferSlots[loc].buffer) {
				++ set.bufferSlots[loc].refCount;
			} else {
				// fill slot with new ImageView
				set.bufferSlots[loc].buffer = it;
				set.bufferSlots[loc].buffer->setLocation(setIdx, loc);
				++ set.bufferSlots[loc].refCount;
				set.usedBufferSlots = std::max(set.usedBufferSlots, loc + 1);
			}

			++ bufferIdx;
		}

		newMaterial->setLayoutIndex(setIdx);

		if (oldImages) {
			auto &oldSet = _layouts[targetSet];
			// remove non-aliased images
			for (auto &oIt : *oldImages) {
				if (oIt.view) {
					-- oldSet.imageSlots[oIt.descriptor].refCount;
					if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
						oldSet.imageSlots[oIt.descriptor].image = nullptr;
					}
					oIt.view = nullptr;
				}
			}
		}
	};

	auto tryToEmplaceSet = [&] (uint32_t setIndex, MaterialLayout &set) -> bool {
		uint32_t emplacedImages = 0;
		uint32_t emplacedBuffers = 0;
		Vector<uint32_t> imagePositions; imagePositions.resize(uniqueImages.size(), maxOf<uint32_t>());
		Vector<uint32_t> bufferPositions; bufferPositions.resize(uniqueBuffers.size(), maxOf<uint32_t>());

		imageIdx = 0;
		// for each unique image, find it's potential place in set
		for (auto &uit : uniqueImages) {
			uint32_t location = 0;
			for (auto &it : set.imageSlots) {
				// check if image can alias with existed
				if (it.image && it.image->getImage() == uit.first->image->image && it.image->getInfo() == uit.first->info) {
					if (imagePositions[imageIdx] == maxOf<uint32_t>()) {
						++ emplacedImages; // mark as emplaced only if not emplaced already
					}
					imagePositions[imageIdx] = location;
					break; // stop searching - best variant
				} else if (!it.image || it.refCount == 0) {
					// only if not emplaced
					if (imagePositions[imageIdx] == maxOf<uint32_t>()) {
						if (std::find(imagePositions.begin(), imagePositions.end(), location) == imagePositions.end()) {
							++ emplacedImages;
							imagePositions[imageIdx] = location;
						}
					}
					// continue searching for possible alias
				}
				++ location;
				if (location > set.usedImageSlots + uniqueImages.size()) {
					break;
				}
			}

			++ imageIdx;
		}

		imageIdx = 0;
		for (auto &uit : uniqueBuffers) {
			uint32_t location = 0;
			for (auto &it : set.bufferSlots) {
				if (it.buffer && it.buffer == uit) {
					if (bufferPositions[imageIdx] == maxOf<uint32_t>()) {
						++ emplacedBuffers; // mark as emplaced only if not emplaced already
					}
					bufferPositions[imageIdx] = location;
					break; // stop searching - best variant
				} else if (!it.buffer || it.refCount == 0) {
					// only if not emplaced
					if (bufferPositions[imageIdx] == maxOf<uint32_t>()) {
						if (std::find(bufferPositions.begin(), bufferPositions.end(), location) == bufferPositions.end()) {
							++ emplacedBuffers;
							bufferPositions[imageIdx] = location;
						}
					}
					// continue searching for possible alias
				}
				++ location;
				if (location > set.usedBufferSlots + uniqueBuffers.size()) {
					break;
				}
			}
			++ imageIdx;
		}

		// if all images emplaced, perform actual emplace and return
		if (emplacedImages == uniqueImages.size() && emplacedBuffers == uniqueBuffers.size()) {
			emplaceMaterial(setIndex, set, imagePositions, bufferPositions);
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
	nIt.imageSlots.resize(_imagesInSet);
	nIt.bufferSlots.resize(_buffersInSet);

	Vector<uint32_t> imageLocations;
	Vector<uint32_t> bufferLocations;
	emplaceMaterial(_layouts.size() - 1, nIt, imageLocations, bufferLocations);
}

bool MaterialImage::canAlias(const MaterialImage &other) const {
	return other.image == image && other.info == info;
}

Material::~Material() {
	if (_ownedData) {
		_images.clear();
		delete _ownedData;
		_ownedData = nullptr;
	}
}

bool Material::init(MaterialId id, const PipelineData *pipeline, Vector<MaterialImage> &&images, Bytes &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = move(images);
	_data = move(data);
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline, const Rc<DynamicImageInstance> &image, Bytes &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = Vector<MaterialImage>({
		MaterialImage{
			.image = &image->data,
			.dynamic = image
		}
	});
	_atlas = image->data.atlas;
	_data = move(data);
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline, const ImageData *image, Bytes &&data, bool ownedData) {
	_id = id;
	_pipeline = pipeline;
	_images = Vector<MaterialImage>({
		MaterialImage({
			image
		})
	});
	_atlas = image->atlas;
	_data = move(data);
	if (ownedData) {
		_ownedData = image;
	}
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline, const ImageData *image, ColorMode mode, Bytes &&data, bool ownedData) {
	_id = id;
	_pipeline = pipeline;

	MaterialImage img({
		image
	});

	img.info.setup(*image);
	img.info.setup(mode);

	_images = Vector<MaterialImage>({
		move(img)
	});
	_atlas = image->atlas;
	_data = move(data);
	if (ownedData) {
		_ownedData = image;
	}
	return true;
}

bool Material::init(const Material *master, Rc<ImageObject> &&image, Rc<DataAtlas> &&atlas, Bytes &&data) {
	_id = master->getId();
	_pipeline = master->getPipeline();
	_data = move(data);

	auto otherData = master->getOwnedData();
	if (!otherData) {
		auto &images = master->getImages();
		if (images.size() > 0) {
			otherData = images[0].image;
		}
	}

	auto ownedData = new ImageData;
	static_cast<ImageInfo &>(*ownedData) = image->getInfo();
	ownedData->image = move(image);
	ownedData->atlas = move(atlas);
	_ownedData = ownedData;

	_images = Vector<MaterialImage>({
		MaterialImage({
			_ownedData
		})
	});
	return true;
}

bool Material::init(const Material *master, Vector<MaterialImage> &&images) {
	_id = master->getId();
	_pipeline = master->getPipeline();
	_data = master->getData().bytes<Interface>();
	_images = move(images);
	for (auto &it : _images) {
		if (it.image->atlas) {
			_atlas = it.image->atlas;
			break;
		}
	}

	return true;
}

void Material::setLayoutIndex(uint32_t idx) {
	_layoutIndex = idx;
}

MaterialAttachment::~MaterialAttachment() { }

bool MaterialAttachment::init(StringView name, const BufferInfo &info, MaterialSet::EncodeCallback &&cb,
		uint32_t size, MaterialType type, Vector<Rc<Material>> &&initials) {
	if (BufferAttachment::init(name, info)) {
		_materialObjectSize = size;
		_type = type;
		_encodeCallback = move(cb);
		_initialMaterials = move(initials);

		for (auto &it : _initialMaterials) {
			it->_id = _attachmentMaterialId.fetch_add(1);
		}
		return true;
	}
	return false;
}

const Rc<gl::MaterialSet> &MaterialAttachment::getMaterials() const {
	return _data;
}

void MaterialAttachment::setMaterials(const Rc<gl::MaterialSet> &data) const {
	auto tmp = _data;
	_data = data;
	if (tmp) {
		tmp->clear();
	}
}

Rc<gl::MaterialSet> MaterialAttachment::allocateSet(const Device &dev) const {
	return Rc<gl::MaterialSet>::create(_info, _encodeCallback, _materialObjectSize,
			dev.getTextureLayoutImagesCount(), dev.getTextureLayoutBuffersCount(), this);
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

void MaterialAttachment::addDynamicTracker(MaterialId id, const Rc<DynamicImage> &image) const {
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		++ it->second.refCount;
	} else {
		it = _dynamicTrackers.emplace(image, DynamicImageTracker{1}).first;
		image->addTracker(this);
	}

	auto iit = it->second.materials.find(id);
	if (iit != it->second.materials.end()) {
		++ iit->second;
	} else {
		it->second.materials.emplace(id, 1);
	}
}

void MaterialAttachment::removeDynamicTracker(MaterialId id, const Rc<DynamicImage> &image) const {
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		auto iit = it->second.materials.find(id);
		if (iit != it->second.materials.end()) {
			-- iit->second;
			if (iit->second == 0) {
				it->second.materials.erase(iit);
			}
		}
		-- it->second.refCount;
		if (it->second.refCount == 0) {
			_dynamicTrackers.erase(it);
			image->removeTracker(this);
		}
	}
}

void MaterialAttachment::updateDynamicImage(Loop &loop, const DynamicImage *image,
		const Vector<Rc<renderqueue::DependencyEvent>> &deps) const {
	auto input = Rc<MaterialInputData>::alloc();
	input->attachment = this;
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		for (auto &materialIt : it->second.materials) {
			input->dynamicMaterialsToUpdate.emplace_back(materialIt.first);
		}
	}
	loop.compileMaterials(move(input), deps);
}

MaterialId MaterialAttachment::getNextMaterialId() const {
	return _attachmentMaterialId.fetch_add(1);
}

auto MaterialAttachment::makeDescriptor(PassData *pass) -> Rc<AttachmentDescriptor> {
	return Rc<MaterialAttachmentDescriptor>::create(pass, this);
}

bool MaterialAttachmentDescriptor::init(PassData *data, Attachment *attachment) {
	if (BufferAttachmentDescriptor::init(data, attachment)) {
		_usesTextureSet = true;
		return true;
	}
	return false;
}

uint64_t MaterialAttachmentDescriptor::getBoundGeneration() const {
	return _boundGeneration.load();
}

void MaterialAttachmentDescriptor::setBoundGeneration(uint64_t id) {
	_boundGeneration.store(id);
}

}
