/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLGlDynamicImage.h"

namespace stappler::xenolith::gl {

DynamicImage::~DynamicImage() { }

bool DynamicImage::init(const Callback<bool(Builder &)> &cb) {
	Builder builder(this);
	if (cb(builder)) {
		return true;
	}
	return false;
}

void DynamicImage::finalize() {
	std::unique_lock<Mutex> lock(_mutex);
	_instance->userdata = nullptr;
	_instance = nullptr;
}

Rc<DynamicImageInstance> DynamicImage::getInstance() {
	std::unique_lock<Mutex> lock(_mutex);
	return _instance;
}

void DynamicImage::updateInstance(Loop &loop, const Rc<ImageObject> &obj, Rc<ImageAtlas> &&atlas, Rc<Ref> &&userdata,
		const Vector<Rc<renderqueue::DependencyEvent>> &deps) {
	auto newInstance = Rc<DynamicImageInstance>::alloc();
	static_cast<ImageInfo &>(newInstance->data) = obj->getInfo();
	newInstance->data.image = obj;
	newInstance->data.atlas = move(atlas);
	newInstance->userdata = move(userdata);
	newInstance->image = this;

	std::unique_lock<Mutex> lock(_mutex);
	if (!_instance) {
		return;
	}

	newInstance->gen = _instance->gen + 1;
	_instance = newInstance;
	_data.image = nullptr;

	for (auto &it : _materialTrackers) {
		it->updateDynamicImage(loop, this, deps);
	}
}

void DynamicImage::addTracker(const MaterialAttachment *a) {
	std::unique_lock<Mutex> lock(_mutex);
	_materialTrackers.emplace(a);
}

void DynamicImage::removeTracker(const MaterialAttachment *a) {
	std::unique_lock<Mutex> lock(_mutex);
	_materialTrackers.erase(a);
}

ImageInfo DynamicImage::getInfo() const {
	std::unique_lock<Mutex> lock(_mutex);
	return ImageInfo(static_cast<const ImageInfo &>(_data));
}

Extent3 DynamicImage::getExtent() const {
	std::unique_lock<Mutex> lock(_mutex);
	if (_instance) {
		return _instance->data.extent;
	} else {
		return _data.extent;
	}
}

void DynamicImage::setImage(const Rc<ImageObject> &obj) {
	std::unique_lock<Mutex> lock(_mutex);

	_data.image = obj;

	auto newInstance = Rc<DynamicImageInstance>::alloc();
	static_cast<ImageInfo &>(newInstance->data) = obj->getInfo();
	newInstance->data.image = obj;
	newInstance->image = this;
	newInstance->gen = 1;

	_instance = newInstance;
}

void DynamicImage::acquireData(const Callback<void(BytesView)> &cb) {
	if (!_data.data.empty()) {
		cb(_data.data);
	} else if (_data.stdCallback) {
		_data.stdCallback(nullptr, 0, cb);
	} else if (_data.memCallback) {
		_data.memCallback(nullptr, 0, cb);
	}
}

const ImageData * DynamicImage::Builder::setImageByRef(StringView key, ImageInfo &&info, BytesView data, Rc<ImageAtlas> &&atlas) {
	static_cast<ImageInfo &>(_data->_data) = info;
	_data->_data.key = key;
	_data->_data.data = data;
	_data->_data.atlas = move(atlas);
	return &_data->_data;
}

const ImageData * DynamicImage::Builder::setImage(StringView key, ImageInfo &&info, FilePath path, Rc<ImageAtlas> &&atlas) {
	String npath;
	if (filesystem::exists(path.get())) {
		npath = path.get().str<Interface>();
	} else if (!filepath::isAbsolute(path.get())) {
		npath = filesystem::currentDir<Interface>(path.get());
		if (!filesystem::exists(npath)) {
			npath.clear();
		}
	}

	if (npath.empty()) {
		log::vtext("Resource", "Fail to add image: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	_data->_keyData = key.str<Interface>();
	static_cast<ImageInfo &>(_data->_data) = info;
	_data->_data.key = _data->_keyData;
	_data->_data.stdCallback = [npath, format = info.format] (uint8_t *ptr, uint64_t size, const ImageData::DataCallback &dcb) {
		Resource::loadImageFileData(ptr, size, npath, format, dcb);
	};;
	_data->_data.atlas = move(atlas);
	return &_data->_data;
}

const ImageData * DynamicImage::Builder::setImage(StringView key, ImageInfo &&info, BytesView data, Rc<ImageAtlas> &&atlas) {
	_data->_keyData = key.str<Interface>();
	_data->_imageData = data.bytes<Interface>();
	static_cast<ImageInfo &>(_data->_data) = info;
	_data->_data.key = _data->_keyData;
	_data->_data.data = _data->_imageData;
	_data->_data.atlas = move(atlas);
	return &_data->_data;
}

const ImageData * DynamicImage::Builder::setImage(StringView key, ImageInfo &&info,
		Function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &&cb, Rc<ImageAtlas> &&atlas) {
	_data->_keyData = key.str<Interface>();
	static_cast<ImageInfo &>(_data->_data) = info;
	_data->_data.key = _data->_keyData;
	_data->_data.stdCallback = cb;
	_data->_data.atlas = move(atlas);
	return &_data->_data;
}

DynamicImage::Builder::Builder(DynamicImage *data) : _data(data) { }

}
