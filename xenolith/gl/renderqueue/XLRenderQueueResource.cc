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

#include "SPBitmap.h"
#include "XLRenderQueueResource.h"

namespace stappler::xenolith::renderqueue {

struct Resource::ResourceData : memory::AllocPool {
	HashTable<gl::BufferData *> buffers;
	HashTable<gl::ImageData *> images;

	const Queue *owner = nullptr;
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

static void Resource_loadImageDirect(uint8_t *glBuffer, uint64_t expectedSize, BytesView encodedImageData, const bitmap::ImageInfo &imageInfo) {
	struct WriteData {
		uint8_t *buffer;
		uint32_t offset;
		uint64_t expectedSize;
	} data;

	data.buffer = glBuffer;
	data.offset = 0;
	data.expectedSize = expectedSize;

	bitmap::BitmapWriter w;
	w.target = &data;

	w.getStride = nullptr;
	w.push = [] (void *ptr, const uint8_t *data, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		memcpy(writeData->buffer + writeData->offset, data, size);
		writeData->offset += size;
	};
	w.resize = [] (void *ptr, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		if (size != writeData->expectedSize) {
			abort();
		}
	};
	w.getData = [] (void *ptr, uint32_t location) {
		auto writeData = ((WriteData *)ptr);
		return writeData->buffer + location;
	};
	w.assign = [] (void *ptr, const uint8_t *data, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		memcpy(writeData->buffer, data, size);
		writeData->offset = size;
	};
	w.clear = [] (void *ptr) { };

	imageInfo.format->load(encodedImageData.data(), encodedImageData.size(), w);
}

static void Resource_loadImageConverted(StringView path, uint8_t *glBuffer, BytesView encodedImageData, const bitmap::ImageInfo &imageInfo, gl::ImageFormat fmt) {
	Bitmap bmp(encodedImageData);

	switch (fmt) {
	case gl::ImageFormat::R8G8B8A8_SRGB:
	case gl::ImageFormat::R8G8B8A8_UNORM:
	case gl::ImageFormat::R8G8B8A8_UINT:
		bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::RGBA8888);
		break;
	case gl::ImageFormat::R8G8B8_SRGB:
	case gl::ImageFormat::R8G8B8_UNORM:
	case gl::ImageFormat::R8G8B8_UINT:
		bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::RGB888);
		break;
	case gl::ImageFormat::R8G8_SRGB:
	case gl::ImageFormat::R8G8_UNORM:
	case gl::ImageFormat::R8G8_UINT:
		bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::IA88);
		break;
	case gl::ImageFormat::R8_SRGB:
	case gl::ImageFormat::R8_UNORM:
	case gl::ImageFormat::R8_UINT:
		if (bmp.alpha() == bitmap::AlphaFormat::Opaque) {
			bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::I8);
		} else {
			bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::A8);
		}
		break;
	default:
		log::vtext("Resource", "loadImageFileData: ", path, ": Invalid image format: ", getImageFormatName(fmt));
		break;
	}
}

static void Resource_loadImageDefault(StringView path, BytesView encodedImageData, gl::ImageFormat fmt, const gl::ImageData::DataCallback &dcb) {
	Bitmap bmp(encodedImageData);

	bool availableFormat = true;
	switch (fmt) {
	case gl::ImageFormat::R8G8B8A8_SRGB:
	case gl::ImageFormat::R8G8B8A8_UNORM:
	case gl::ImageFormat::R8G8B8A8_UINT:
		bmp.convert(bitmap::PixelFormat::RGBA8888);
		break;
	case gl::ImageFormat::R8G8B8_SRGB:
	case gl::ImageFormat::R8G8B8_UNORM:
	case gl::ImageFormat::R8G8B8_UINT:
		bmp.convert(bitmap::PixelFormat::RGB888);
		break;
	case gl::ImageFormat::R8G8_SRGB:
	case gl::ImageFormat::R8G8_UNORM:
	case gl::ImageFormat::R8G8_UINT:
		bmp.convert(bitmap::PixelFormat::IA88);
		break;
	case gl::ImageFormat::R8_SRGB:
	case gl::ImageFormat::R8_UNORM:
	case gl::ImageFormat::R8_UINT:
		bmp.convert(bitmap::PixelFormat::A8);
		break;
	default:
		availableFormat = false;
		log::vtext("Resource", "loadImageFileData: ", path, ": Invalid image format: ", getImageFormatName(fmt));
		break;
	}

	if (availableFormat) {
		dcb(BytesView(bmp.dataPtr(), bmp.data().size()));
	} else {
		dcb(BytesView());
	}
}

void Resource::loadImageFileData(uint8_t *ptr, uint64_t expectedSize, StringView path, gl::ImageFormat fmt, const gl::ImageData::DataCallback &dcb) {
	auto p = memory::pool::create(memory::pool::acquire());
	memory::pool::push(p);
	auto f = filesystem::openForReading(path);
	if (f) {
		auto fsize = f.size();
		auto mem = (uint8_t *)memory::pool::palloc(p, fsize);
		f.seek(0, io::Seek::Set);
		f.read(mem, fsize);
		f.close();

		bitmap::ImageInfo info;
		if (!bitmap::getImageInfo(BytesView(mem, fsize), info)) {
			log::vtext("Resource", "loadImageFileData: ", path, ": fail to read image info");
		} else {
			if (ptr) {
				// check if we can load directly into GL memory
				switch (info.color) {
				case bitmap::PixelFormat::RGBA8888:
					switch (fmt) {
					case gl::ImageFormat::R8G8B8A8_SRGB:
					case gl::ImageFormat::R8G8B8A8_UNORM:
					case gl::ImageFormat::R8G8B8A8_UINT:
						Resource_loadImageDirect(ptr, expectedSize, BytesView(mem, fsize), info);
						break;
					default:
						Resource_loadImageConverted(path, ptr, BytesView(mem, fsize), info, fmt);
						break;
					}
					break;
				case bitmap::PixelFormat::RGB888:
					switch (fmt) {
					case gl::ImageFormat::R8G8B8_SRGB:
					case gl::ImageFormat::R8G8B8_UNORM:
					case gl::ImageFormat::R8G8B8_UINT:
						Resource_loadImageDirect(ptr, expectedSize, BytesView(mem, fsize), info);
						break;
					default:
						Resource_loadImageConverted(path, ptr, BytesView(mem, fsize), info, fmt);
						break;
					}
					break;
				case bitmap::PixelFormat::IA88:
					switch (fmt) {
					case gl::ImageFormat::R8G8_SRGB:
					case gl::ImageFormat::R8G8_UNORM:
					case gl::ImageFormat::R8G8_UINT:
						Resource_loadImageDirect(ptr, expectedSize, BytesView(mem, fsize), info);
						break;
					default:
						Resource_loadImageConverted(path, ptr, BytesView(mem, fsize), info, fmt);
						break;
					}
					break;
				case bitmap::PixelFormat::I8:
				case bitmap::PixelFormat::A8:
					switch (fmt) {
					case gl::ImageFormat::R8_SRGB:
					case gl::ImageFormat::R8_UNORM:
					case gl::ImageFormat::R8_UINT:
						Resource_loadImageDirect(ptr, expectedSize, BytesView(mem, fsize), info);
						break;
					default:
						Resource_loadImageConverted(path, ptr, BytesView(mem, fsize), info, fmt);
						break;
					}
					break;
				default:
					log::vtext("Resource", "loadImageFileData: ", path, ": Unknown format");
					dcb(BytesView());
					break;
				}
			} else {
				// use data callback
				Resource_loadImageDefault(path, BytesView(mem, fsize), fmt, dcb);
			}
		}
	} else {
		log::vtext("Resource", "loadImageFileData: ", path, ": fail to load file");
		dcb(BytesView());
	}
	memory::pool::pop();
	memory::pool::destroy(p);
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

const Queue *Resource::getOwner() const {
	return _data->owner;
}

void Resource::setOwner(const Queue *q) {
	_data->owner = q;
}

const HashTable<gl::BufferData *> &Resource::getBuffers() const {
	return _data->buffers;
}

const HashTable<gl::ImageData *> &Resource::getImages() const {
	return _data->images;
}

const gl::BufferData *Resource::getBuffer(StringView key) const {
	return _data->buffers.get(key);
}

const gl::ImageData *Resource::getImage(StringView key) const {
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

static void Resource_loadFileData(uint8_t *ptr, uint64_t size, StringView path, const gl::BufferData::DataCallback &dcb) {
	auto p = memory::pool::create(memory::pool::acquire());
	memory::pool::push(p);
	auto f = filesystem::openForReading(path);
	if (f) {
		uint64_t fsize = f.size();
		f.seek(0, io::Seek::Set);
		if (ptr) {
			f.read(ptr, std::min(fsize, size));
			f.close();
		} else {
			auto mem = (uint8_t *)memory::pool::palloc(p, fsize);
			f.read(mem, fsize);
			f.close();
			dcb(BytesView(mem, fsize));
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

const gl::BufferData *Resource::Builder::addBufferByRef(StringView key, gl::BufferInfo &&info, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::BufferData>(_data->buffers, key, [&] () -> gl::BufferData * {
		auto buf = new (_data->pool) gl::BufferData();
		static_cast<gl::BufferInfo &>(*buf) = move(info);
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
const gl::BufferData *Resource::Builder::addBuffer(StringView key, gl::BufferInfo &&info, FilePath path) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

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
		log::vtext("Resource", "Fail to add buffer: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::BufferData>(_data->buffers, key, [&] () -> gl::BufferData * {
		auto fpath = StringView(npath).pdup(_data->pool);
		auto buf = new (_data->pool) gl::BufferData;
		static_cast<gl::BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->callback = [fpath] (uint8_t *ptr, uint64_t size, const gl::BufferData::DataCallback &dcb) {
			Resource_loadFileData(ptr, size, fpath, dcb);
		};
		filesystem::Stat stat;
		if (filesystem::stat(path.get(), stat)) {
			buf->size = stat.size;
		}
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const gl::BufferData *Resource::Builder::addBuffer(StringView key, gl::BufferInfo &&info, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::BufferData>(_data->buffers, key, [&] () -> gl::BufferData * {
		auto buf = new (_data->pool) gl::BufferData();
		static_cast<gl::BufferInfo &>(*buf) = move(info);
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
const gl::BufferData *Resource::Builder::addBuffer(StringView key, gl::BufferInfo &&info, size_t size,
		const memory::function<void(uint8_t *, uint64_t, const gl::BufferData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::BufferData>(_data->buffers, key, [&] () -> gl::BufferData * {
		auto buf = new (_data->pool) gl::BufferData;
		static_cast<gl::BufferInfo &>(*buf) = move(info);
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

const gl::ImageData *Resource::Builder::addImage(StringView key, gl::ImageInfo &&img, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::ImageData>(_data->images, key, [&] () -> gl::ImageData * {
		auto buf = new (_data->pool) gl::ImageData;
		static_cast<gl::ImageInfo &>(*buf) = move(img);
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
const gl::ImageData *Resource::Builder::addImage(StringView key, gl::ImageInfo &&img, FilePath path) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

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

	Extent3 extent;
	extent.depth = 1;
	if (!bitmap::getImageSize(StringView(npath), extent.width, extent.height)) {
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::ImageData>(_data->images, key, [&] () -> gl::ImageData * {
		auto fpath = StringView(npath).pdup(_data->pool);
		auto buf = new (_data->pool) gl::ImageData;
		static_cast<gl::ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [fpath, format = img.format] (uint8_t *ptr, uint64_t size, const gl::ImageData::DataCallback &dcb) {
			Resource::loadImageFileData(ptr, size, fpath, format, dcb);
		};
		buf->extent = extent;
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
 	return p;
}
const gl::ImageData *Resource::Builder::addImageByRef(StringView key, gl::ImageInfo &&img, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::ImageData>(_data->images, key, [&] () -> gl::ImageData * {
		auto buf = new (_data->pool) gl::ImageData;
		static_cast<gl::ImageInfo &>(*buf) = move(img);
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
const gl::ImageData *Resource::Builder::addImage(StringView key, gl::ImageInfo &&img,
		const memory::function<void(uint8_t *, uint64_t, const gl::ImageData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<gl::ImageData>(_data->images, key, [&] () -> gl::ImageData * {
		auto buf = new (_data->pool) gl::ImageData;
		static_cast<gl::ImageInfo &>(*buf) = move(img);
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
