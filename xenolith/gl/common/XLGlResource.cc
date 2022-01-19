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

#include "XLGlResource.h"
#include "SPBitmap.h"

namespace stappler::xenolith::gl {

struct Resource::ResourceData : memory::AllocPool {
	HashTable<BufferData *> buffers;
	HashTable<ImageData *> images;

	const RenderQueue *owner = nullptr;
	bool compiled = false;
	StringView key;
	memory::pool_t *pool = nullptr;

	void clear() {
		compiled = false;
		for (auto &it : buffers) {
			it->buffer = nullptr;
		}
		for (auto &it : images) {
			it->image = nullptr;
		}
	}
};

Resource::Resource() { }

Resource::~Resource() {
	if (_data) {
		_data->clear();
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool Resource::init(Builder && buf) {
	_data = buf._data;
	buf._data = nullptr;
	for (auto &it : _data->images) {
		it->resource = this;
	}
	for (auto &it : _data->buffers) {
		it->resource = this;
	}
	return true;
}

void Resource::clear() {
	_data->clear();
}

bool Resource::isCompiled() const {
	return _data->compiled;
}

void Resource::setCompiled(bool value) {
	_data->compiled = value;
}

const RenderQueue *Resource::getOwner() const {
	return _data->owner;
}

void Resource::setOwner(const RenderQueue *q) {
	_data->owner = q;
}

const HashTable<BufferData *> &Resource::getBuffers() const {
	return _data->buffers;
}

const HashTable<ImageData *> &Resource::getImages() const {
	return _data->images;
}

const BufferData *Resource::getBuffer(StringView key) const {
	return _data->buffers.get(key);
}

const ImageData *Resource::getImage(StringView key) const {
	return _data->images.get(key);
}

StringView Resource::getName() const {
	return _data->key;
}

memory::pool_t *Resource::getPool() const {
	return _data->pool;
}


template <typename T>
static T * Resource_conditionalInsert(HashTable<T *> &vec, StringView key, const Callback<T *()> &cb, memory::pool_t *pool) {
	auto it = vec.find(key);
	if (it == vec.end()) {
		T *obj = nullptr;
		perform([&] {
			obj = cb();
		}, pool);
		if (obj) {
			return *vec.emplace(obj).first;
		}
	}
	return nullptr;
}

template <typename T>
static T * Resource_conditionalInsert(memory::vector<T *> &vec, StringView key, const Callback<T *()> &cb, memory::pool_t *pool) {
	T *obj = nullptr;
	perform([&] {
		obj = cb();
	}, pool);
	if (obj) {
		return vec.emplace_back(obj);
	}
	return nullptr;
}

static void Resource_loadFileData(StringView path, const BufferData::DataCallback &dcb) {
	auto p = memory::pool::create(memory::pool::acquire());
	memory::pool::push(p);
	auto f = filesystem::openForReading(path);
	if (f) {
		auto fsize = f.size();
		auto mem = (uint8_t *)memory::pool::palloc(p, fsize);
		f.seek(0, io::Seek::Set);
		f.read(mem, fsize);
		f.close();

		dcb(BytesView(mem, fsize));
	} else {
		dcb(BytesView());
	}
	memory::pool::pop();
	memory::pool::destroy(p);
};

static void Resource_loadImageFileData(StringView path, ImageFormat fmt, const BufferData::DataCallback &dcb) {
	auto p = memory::pool::create(memory::pool::acquire());
	memory::pool::push(p);
	auto f = filesystem::openForReading(path);
	if (f) {
		auto fsize = f.size();
		auto mem = (uint8_t *)memory::pool::palloc(p, fsize);
		f.seek(0, io::Seek::Set);
		f.read(mem, fsize);
		f.close();

		Bitmap bmp(mem, fsize);

		bool availableFormat = true;
		switch (fmt) {
		case ImageFormat::R8G8B8A8_SRGB:
		case ImageFormat::R8G8B8A8_UNORM:
		case ImageFormat::R8G8B8A8_UINT:
			bmp.convert(Bitmap::PixelFormat::RGBA8888);
			break;
		case ImageFormat::R8G8B8_SRGB:
		case ImageFormat::R8G8B8_UNORM:
		case ImageFormat::R8G8B8_UINT:
			bmp.convert(Bitmap::PixelFormat::RGB888);
			break;
		case ImageFormat::R8G8_SRGB:
		case ImageFormat::R8G8_UNORM:
		case ImageFormat::R8G8_UINT:
			bmp.convert(Bitmap::PixelFormat::IA88);
			break;
		case ImageFormat::R8_SRGB:
		case ImageFormat::R8_UNORM:
		case ImageFormat::R8_UINT:
			bmp.convert(Bitmap::PixelFormat::A8);
			break;
		default:
			availableFormat = false;
			log::vtext("Resource", "Invalid image format: ", getImageFormatName(fmt));
			break;
		}

		if (availableFormat) {
			dcb(BytesView(bmp.dataPtr(), bmp.data().size()));
		} else {
			dcb(BytesView());
		}
	} else {
		dcb(BytesView());
	}
	memory::pool::pop();
	memory::pool::destroy(p);
};

Resource::Builder::Builder(StringView name) {
	auto p = memory::pool::create((memory::pool_t *)nullptr);
	memory::pool::push(p);
	_data = new (p) ResourceData;
	_data->pool = p;
	_data->key = name.pdup(p);
	memory::pool::pop();
}

Resource::Builder::~Builder() {
	if (_data) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

const BufferData *Resource::Builder::addBufferByRef(StringView key, BufferInfo &&info, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData();
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		buf->size = data.size();
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info, FilePath path) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	String npath;
	if (filesystem::exists(path.get())) {
		npath = path.get().str();
	} else if (!filepath::isAbsolute(path.get())) {
		npath = filesystem::currentDir(path.get());
		if (!filesystem::exists(npath)) {
			npath.clear();
		}
	}

	if (npath.empty()) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto fpath = StringView(npath).pdup(_data->pool);
		auto buf = new (_data->pool) BufferData;
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->callback = [fpath] (const BufferData::DataCallback &dcb) {
			Resource_loadFileData(fpath, dcb);
		};
		buf->size = filesystem::size(path.get());
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData();
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		buf->size = data.size();
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info, size_t size,
		const memory::function<void(const BufferData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData;
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->size = size;
		buf->key = key.pdup(_data->pool);
		buf->callback = cb;
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}
const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img, FilePath path) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	String npath;
	if (filesystem::exists(path.get())) {
		npath = path.get().str();
	} else if (!filepath::isAbsolute(path.get())) {
		npath = filesystem::currentDir(path.get());
		if (!filesystem::exists(npath)) {
			npath.clear();
		}
	}

	if (npath.empty()) {
		log::vtext("Resource", "Fail to add image: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	size_t width, height, depth = 1;
	if (!Bitmap::getImageSize(StringView(npath), width, height)) {
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto fpath = StringView(npath).pdup(_data->pool);
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [fpath, format = img.format] (const ImageData::DataCallback &dcb) {
			Resource_loadImageFileData(fpath, format, dcb);
		};
		buf->extent = Extent3(width, height, depth);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
 	return p;
}
const ImageData *Resource::Builder::addImageByRef(StringView key, ImageInfo &&img, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}
const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img,
		const memory::function<void(const ImageData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = cb;
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

}
