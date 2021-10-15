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

#include "XLGlObject.h"
#include "XLGlDevice.h"

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

ImageViewInfo ImageObject::getViewInfo(const ImageViewInfo &info) const {
	ImageViewInfo ret(info);
	if (ret.format == ImageFormat::Undefined) {
		ret.format = _info.format;
	}
	if (ret.layerCount.get() == maxOf<uint32_t>()) {
		ret.layerCount = ArrayLayers(_info.arrayLayers.get() - ret.baseArrayLayer.get());
	}
	return ret;
}

static std::atomic<uint64_t> s_ImagerViewCurrentIndex = 1;

bool ImageView::init(Device &dev, ClearCallback cb, ObjectType type, void *ptr) {
	if (Object::init(dev, cb, type, ptr)) {
		_index = s_ImagerViewCurrentIndex.fetch_add(1);
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

}
