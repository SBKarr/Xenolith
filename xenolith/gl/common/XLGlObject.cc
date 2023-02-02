/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLGlObject.h"
#include "XLGlDevice.h"

#include "spirv_reflect.h"

namespace stappler::xenolith::gl {

bool ObjectInterface::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr) {
	_device = &dev;
	_callback = cb;
	_type = type;
	_handle = ptr;
	_device->addObject(this);
	return true;
}

void ObjectInterface::invalidate() {
	if (_callback) {
		_callback(_device, _type, _handle);
		_device->removeObject(this);
		_callback = nullptr;
		_device = nullptr;
		_handle = ObjectHandle::zero();
	}
}

NamedObject::~NamedObject() {
	invalidate();
}

Object::~Object() {
	invalidate();
}

static std::atomic<uint64_t> s_RenderPassImplCurrentIndex = 1;

bool RenderPass::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr) {
	if (NamedObject::init(dev, cb, type, ptr)) {
		_index = s_RenderPassImplCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}

uint64_t Framebuffer::getViewHash(SpanView<Rc<ImageView>> views) {
	Vector<uint64_t> ids; ids.reserve(views.size());
	for (auto &it : views) {
		ids.emplace_back(it->getIndex());
	}
	return getViewHash(ids);
}

uint64_t Framebuffer::getViewHash(SpanView<uint64_t> ids) {
	return hash::hash64((const char *)ids.data(), ids.size() * sizeof(uint64_t));
}

uint64_t Framebuffer::getHash() const {
	return hash::hash64((const char *)_viewIds.data(), _viewIds.size() * sizeof(uint64_t));
}

static std::atomic<uint64_t> s_ImageViewCurrentIndex = 1;

bool ImageAtlas::init(uint32_t count, uint32_t objectSize, Extent2 imageSize) {
	_objectSize = objectSize;
	_imageExtent = imageSize;
	_names.reserve(count);
	_data.reserve(count * objectSize);
	return true;
}

uint8_t *ImageAtlas::getObjectByName(uint32_t id) {
	auto it = _names.find(id);
	if (it != _names.end()) {
		if (it->second < _data.size() / _objectSize) {
			return _data.data() + _objectSize * it->second;
		}
	}
	return nullptr;
}

uint8_t *ImageAtlas::getObjectByOrder(uint32_t order) {
	if (order < _data.size() / _objectSize) {
		return _data.data() + _objectSize * order;
	}
	return nullptr;
}

void ImageAtlas::addObject(uint32_t id, void *data) {
	auto off = _data.size();
	_data.resize(off + _objectSize);
	memcpy(_data.data() + off, data,  _objectSize);

	_names.emplace(id, off / _objectSize);
}

bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}
bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr, uint64_t idx) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = idx;
		return true;
	}
	return false;
}

ImageViewInfo ImageObject::getViewInfo(const ImageViewInfo &info) const {
	return _info.getViewInfo(info);
}

ImageView::~ImageView() {
	if (_releaseCallback) {
		_releaseCallback();
		_releaseCallback = nullptr;
	}
}

bool ImageView::init(Device &dev, ClearCallback cb, ObjectType type, ObjectHandle ptr) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}

void ImageView::invalidate() {
	if (_releaseCallback) {
		_releaseCallback();
		_releaseCallback = nullptr;
	}
	Object::invalidate();
}

void ImageView::setReleaseCallback(Function<void()> &&cb) {
	_releaseCallback = move(cb);
}

void ImageView::runReleaseCallback() {
	if (_releaseCallback) {
		auto cb = move(_releaseCallback);
		_releaseCallback = nullptr;
		cb();
	}
}

Extent3 ImageView::getExtent() const {
	return _image->getInfo().extent;
}

void TextureSet::write(const MaterialLayout &set) {
	_layoutIndexes.clear();
	for (uint32_t i = 0; i < set.usedSlots; ++ i) {
		if (set.slots[i].image) {
			_layoutIndexes.emplace_back(set.slots[i].image->getIndex());
		} else {
			_layoutIndexes.emplace_back(0);
		}
	}

	_layoutIndexes.resize(_count, 0);
}

void Shader::inspectShader(SpanView<uint32_t> data) {
	SpvReflectShaderModule shader;

	spvReflectCreateShaderModule(data.size() * sizeof(uint32_t), data.data(), &shader);

	ProgramStage stage = ProgramStage::None;
	switch (shader.spirv_execution_model) {
	case SpvExecutionModelVertex: stage = ProgramStage::Vertex; break;
	case SpvExecutionModelTessellationControl: stage = ProgramStage::TesselationControl; break;
	case SpvExecutionModelTessellationEvaluation: stage = ProgramStage::TesselationEvaluation; break;
	case SpvExecutionModelGeometry: stage = ProgramStage::Geometry; break;
	case SpvExecutionModelFragment: stage = ProgramStage::Fragment; break;
	case SpvExecutionModelGLCompute: stage = ProgramStage::Compute; break;
	case SpvExecutionModelKernel: stage = ProgramStage::Compute; break;
	case SpvExecutionModelTaskNV: stage = ProgramStage::Task; break;
	case SpvExecutionModelMeshNV: stage = ProgramStage::Mesh; break;
	case SpvExecutionModelRayGenerationKHR: stage = ProgramStage::RayGen; break;
	case SpvExecutionModelIntersectionKHR: stage = ProgramStage::Intersection; break;
	case SpvExecutionModelAnyHitKHR: stage = ProgramStage::AnyHit; break;
	case SpvExecutionModelClosestHitKHR: stage = ProgramStage::ClosestHit; break;
	case SpvExecutionModelMissKHR: stage = ProgramStage::MissHit; break;
	case SpvExecutionModelCallableKHR: stage = ProgramStage::Callable; break;
	default: break;
	}

	std::cout << "[" << getProgramStageDescription(stage) << "]\n";

	for (auto &it : makeSpanView(shader.descriptor_bindings, shader.descriptor_binding_count)) {
		std::cout << "Binging: [" << it.set << ":" << it.binding << "] "
				<< renderqueue::getDescriptorTypeName(DescriptorType(it.descriptor_type)) << "\n";
	}

	for (auto &it : makeSpanView(shader.push_constant_blocks, shader.push_constant_block_count)) {
		std::cout << "PushConstant: [" << it.absolute_offset << " - " << it.padded_size << "]\n";
	}

	spvReflectDestroyShaderModule(&shader);
}

void Shader::inspect(SpanView<uint32_t> data) {
	SpvReflectShaderModule shader;

	spvReflectCreateShaderModule(data.size() * sizeof(uint32_t), data.data(), &shader);

	uint32_t count = 0;
	SpvReflectDescriptorBinding *bindings = nullptr;

	spvReflectEnumerateDescriptorBindings(&shader, &count, &bindings);

	for (auto &it : makeSpanView(shader.descriptor_bindings, shader.descriptor_binding_count)) {
		std::cout << "[" << it.set << ":" << it.binding << "] "
				<< renderqueue::getDescriptorTypeName(DescriptorType(it.descriptor_type)) << "\n";
	}

	spvReflectDestroyShaderModule(&shader);
}

void Semaphore::setSignaled(bool value) {
	_signaled = value;
}

void Semaphore::setWaited(bool value) {
	_waited = value;
}

void Semaphore::setInUse(bool value, uint64_t timeline) {
	if (timeline == _timeline) {
		_inUse = value;
	}
}

bool Semaphore::reset() {
	if (_signaled == _waited) {
		_signaled = false;
		_waited = false;
		_inUse = false;
		++ _timeline;
		return true;
	}
	return false;
}

}
