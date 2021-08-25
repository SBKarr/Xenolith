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

namespace stappler::xenolith::gl {

struct Resource::ResourceData : memory::AllocPool {
	HashTable<BufferData *> buffers;
	HashTable<ImageData *> images;

	StringView name;
	memory::pool_t *pool = nullptr;
};

Resource::Resource() { }

Resource::~Resource() {
	if (_data) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool Resource::init(Builder && buf) {
	_data = buf._data;
	buf._data = nullptr;
	return true;
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

void Resource::setInUse(bool v) {
	_inUse = v;
}

void Resource::setActive(bool v) {
	_active = v;
}

StringView Resource::getName() const {
	return _data->name;
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

static void Resource_loadFileData(FilePath path, const BufferData::DataCallback &dcb) {
	auto p = memory::pool::create(memory::pool::acquire());
	memory::pool::push(p);
	auto f = filesystem::openForReading(path.get());
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

Resource::Builder::Builder(StringView name) {
	auto p = memory::pool::create((memory::pool_t *)nullptr);
	memory::pool::push(p);
	_data = new (p) ResourceData;
	_data->pool = p;
	_data->name = name.pdup(p);
	memory::pool::pop();
}

Resource::Builder::~Builder() {
	if (_data) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

BufferData *Resource::Builder::doAddBuffer(StringView key, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData;
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		buf->size = data.size();
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

BufferData *Resource::Builder::doAddBuffer(StringView key, FilePath path) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	if (!filesystem::exists(path.get())) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto fpath = FilePath(path.get().pdup(_data->pool));
		auto buf = new (_data->pool) BufferData;
		buf->key = key.pdup(_data->pool);
		buf->callback = [&] (const BufferData::DataCallback &dcb) {
			Resource_loadFileData(fpath, dcb);
		};
		buf->size = filesystem::size(path.get());
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
BufferData *Resource::Builder::doAddBufferByRef(StringView key, BytesView data) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData;
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		buf->size = data.size();
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

BufferData *Resource::Builder::doAddBuffer(StringView key, size_t size, const memory::function<void(const BufferData::DataCallback &)> &cb) {
	if (!_data) {
		log::vtext("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key, [&] () -> BufferData * {
		auto buf = new (_data->pool) BufferData;
		buf->key = key.pdup(_data->pool);
		buf->callback = cb;
		buf->size = size;
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

ImageData *Resource::Builder::doAddImage(StringView key, BytesView data, ImageInfo &&img) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		static_cast<ImageInfo &>(*buf) = move(img);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

ImageData *Resource::Builder::doAddImage(StringView key, FilePath path, ImageInfo &&img) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	if (!filesystem::exists(path.get())) {
		log::vtext("Resource", "Fail to add image: ", key, ", file not found: ", path.get());
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto fpath = FilePath(path.get().pdup(_data->pool));
		auto buf = new (_data->pool) ImageData;
		buf->key = key.pdup(_data->pool);
		buf->callback = [&] (const ImageData::DataCallback &dcb) {
			Resource_loadFileData(fpath, dcb);
		};
		static_cast<ImageInfo &>(*buf) = move(img);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

ImageData *Resource::Builder::doAddImageByRef(StringView key, BytesView data, ImageInfo &&img) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		static_cast<ImageInfo &>(*buf) = move(img);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

ImageData *Resource::Builder::doAddImage(StringView key, const memory::function<void(const BufferData::DataCallback &)> &cb, ImageInfo &&img) {
	if (!_data) {
		log::vtext("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&] () -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		buf->key = key.pdup(_data->pool);
		buf->callback = cb;
		static_cast<ImageInfo &>(*buf) = move(img);
		return buf;
	}, _data->pool);
	if (!p) {
		log::vtext("Resource", _data->name, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

void Resource::Builder::setBufferOption(BufferData &b, BufferFlags flags) {
	b.flags |= flags;
}
void Resource::Builder::setBufferOption(BufferData &b, BufferUsage usage) {
	b.usage |= usage;
}
void Resource::Builder::setBufferOption(BufferData &b, uint64_t size) {
	b.size = std::max(size, b.size);
}

}
