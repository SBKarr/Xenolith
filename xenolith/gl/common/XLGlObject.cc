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

#include "XLGlObject.h"
#include "XLGlDevice.h"

#include "spirv_reflect.h"

namespace stappler::xenolith::gl {

bool ObjectInterface::init(Device &dev, ClearCallback cb, ObjectType type, void *ptr) {
	_device = &dev;
	_callback = cb;
	_type = type;
	_ptr = ptr;
	_device->addObject(this);
	return true;
}

void ObjectInterface::invalidate() {
	if (_callback) {
		_callback(_device, _type, _ptr);
		_device->removeObject(this);
		_callback = nullptr;
		_device = nullptr;
		_ptr = nullptr;
	}
}

NamedObject::~NamedObject() {
	invalidate();
}

Object::~Object() {
	invalidate();
}

static std::atomic<uint64_t> s_ImageViewCurrentIndex = 1;

bool ImageAtlas::init(size_t count) {
	_names.reserve(count);
	_objects.reserve(count);
	return true;
}

Vec2 ImageAtlas::getObjectByName(uint32_t id) const {
	auto it = _names.find(id);
	if (it != _names.end()) {
		if (it->second < _objects.size()) {
			return _objects[it->second];
		}
	}
	return Vec2();
}

Vec2 ImageAtlas::getObjectByOrder(uint32_t id) const {
	if (id < _objects.size()) {
		return _objects[id];
	}
	return Vec2();
}

void ImageAtlas::addObject(uint32_t idx, Vec2 obj) {
	_objects.emplace_back(obj);
	auto num = _objects.size() - 1;
	_names.emplace(idx, num);
}

bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, void *ptr) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
}
bool ImageObject::init(Device &dev, ClearCallback cb, ObjectType type, void *ptr, uint64_t idx) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = idx;
		return true;
	}
	return false;
}

ImageViewInfo ImageObject::getViewInfo(const ImageViewInfo &info) const {
	return _info.getViewInfo(info);
}

bool ImageView::init(Device &dev, ClearCallback cb, ObjectType type, void *ptr) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = s_ImageViewCurrentIndex.fetch_add(1);
		return true;
	}
	return false;
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
		std::cout << "Binging: [" << it.set << ":" << it.binding << "] " << getDescriptorTypeName(DescriptorType(it.descriptor_type)) << "\n";
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
		std::cout << "[" << it.set << ":" << it.binding << "] " << getDescriptorTypeName(DescriptorType(it.descriptor_type)) << "\n";
	}

	spvReflectDestroyShaderModule(&shader);
}

}
