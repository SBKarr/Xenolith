/**
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

#include "XLObjBundleFileBase.h"
#include "XLObjFile.h"

namespace stappler::xenolith::obj {

ObjBundleFileBase::~ObjBundleFileBase() { }

bool ObjBundleFileBase::init(OpenMode m) {
	_mode = m;
	return true;
}

bool ObjBundleFileBase::init(FilePath path, OpenMode m) {
	_mode = m;
	return readFile(path, m == OpenMode::Read);
}

bool ObjBundleFileBase::init(BytesView data, OpenMode m) {
	_mode = m;
	return readFile(data);
}

void ObjBundleFileBase::addObject(const ObjFile &file) {
	addObject(file, file.getName());
}

void ObjBundleFileBase::addObject(const ObjFile &file, StringView name) {
	uint32_t startIndex = _indexes.size();

	for (const ObjFile::Face &it : file.getFaces()) {
		if (it.values.size() == 3) {
			for (auto &iit : it.values) {
				Vertex v;
				if (auto pos = file.getPosition(iit)) {
					v.pos = *pos;
				}
				if (auto norm = file.getNormal(iit)) {
					v.norm = *norm;
				}
				if (auto tex = file.getTexture(iit)) {
					v.tex = tex->xy();
				}

				if (auto vertex = findVertex(v)) {
					_indexes.emplace_back(vertex - _vertexes.data());
				} else {
					_indexes.emplace_back(_vertexes.size());
					_vertexes.emplace_back(v);
				}
			}
		} else {
			// todo: tesselate face with more than 3 values
		}
	}

	if (_indexes.size() - startIndex > 0) {
		uint32_t nameStart = 0;
		if (!name.empty()) {
			nameStart = _strings.size();
			_strings.resize(nameStart + name.size());
			memcpy(_strings.data() + nameStart, name.data(), name.size());
		}
		_objects.emplace_back(Object({
			startIndex, uint32_t(_indexes.size() - startIndex),
			nameStart, uint32_t(name.size())
		}));
	}
}

bool ObjBundleFileBase::save(FilePath path, BlockFlags flags) const {
	auto file = filesystem::File(filesystem::native::fopen_fn(path.get(), "wb"));
	if (!file) {
		log::vtext("ObjStorageFile", "Fail to open file for writing: ", path.get());
		return false;
	}

	FileStruct fstruct;
	setup(fstruct);

	if (file.xsputn((const char *)&fstruct, sizeof(FileStruct)) != sizeof(FileStruct)) {
		log::vtext("ObjStorageFile", "Fail write file header: ", path.get());
		return false;
	}

	if (file.xsputn((const char *)_objects.data(), sizeof(Object) * _objects.size()) != ssize_t(sizeof(Object) * _objects.size())) {
		log::vtext("ObjStorageFile", "Fail write objects: ", path.get());
		return false;
	}

	if (file.xsputn((const char *)_strings.data(), sizeof(char) * _strings.size()) != ssize_t(sizeof(char) * _strings.size())) {
		log::vtext("ObjStorageFile", "Fail write strings: ", path.get());
		return false;
	}

	if (file.xsputn((const char *)_vertexes.data(), sizeof(Vertex) * _vertexes.size()) != ssize_t(sizeof(Vertex) * _vertexes.size())) {
		log::vtext("ObjStorageFile", "Fail write vertexes: ", path.get());
		return false;
	}

	if (file.xsputn((const char *)_indexes.data(), sizeof(Index) * _indexes.size()) != ssize_t(sizeof(Index) * _indexes.size())) {
		log::vtext("ObjStorageFile", "Fail write vertexes: ", path.get());
		return false;
	}

	file.close();

	return true;
}

ObjBundleFileBase::Interface::BytesType ObjBundleFileBase::save(BlockFlags flags) const {
	FileStruct fstruct;
	setup(fstruct);

	Interface::BytesType ret;
	ret.resize(fstruct.header.fileSize);

	memcpy(ret.data(), &fstruct, sizeof(FileStruct));
	memcpy(ret.data() + fstruct.blocks[toInt(BlockElementType::Object)].fileOffset, _objects.data(), sizeof(Object) * _objects.size());
	memcpy(ret.data() + fstruct.blocks[toInt(BlockElementType::String)].fileOffset, _strings.data(), sizeof(char) * _strings.size());
	memcpy(ret.data() + fstruct.blocks[toInt(BlockElementType::Vertex)].fileOffset, _vertexes.data(), sizeof(Vertex) * _vertexes.size());
	memcpy(ret.data() + fstruct.blocks[toInt(BlockElementType::Index)].fileOffset, _indexes.data(), sizeof(Index) * _indexes.size());

	return ret;
}

template <typename T, typename Callback>
static bool ObjStorageFile_readBlock(filesystem::File &file, const ObjBundleFileBase::BlockHeader &block, const Callback &cb) {
	if (block.fileSize > 0 && block.eltCount > 0) {
		if ((block.flags & ObjBundleFileBase::BlockFlags::Compressed) != ObjBundleFileBase::BlockFlags::None) {
			file.seek(block.fileOffset, io::Seek::Set);
			ObjBundleFileBase::Bytes bytes; bytes.resize(block.fileSize);
			if (file.read(bytes.data(), block.fileSize) != block.fileSize) {
				return false;
			}

			if (auto s = data::getDecompressedSize(bytes.data(), bytes.size())) {
				if (s / sizeof(T) == block.eltCount) {
					auto d = cb(block.eltCount);
					return data::decompress(bytes.data(), bytes.size(), (uint8_t *)d, s) == s;
				} else {
					return false;
				}
			} else if ( block.fileSize / sizeof(T) == block.eltCount) {
				auto d = cb(block.eltCount);
				memcpy((uint8_t *)d, bytes.data(), block.fileSize);
				return true;
			}
		}

		if ( block.fileSize / sizeof(T) == block.eltCount) {
			auto d = cb(block.eltCount);
			file.seek(block.fileOffset, io::Seek::Set);
			return file.read((uint8_t *)d, block.fileSize) == block.fileSize;
		}
	}

	return false;
}

bool ObjBundleFileBase::readFile(FilePath ipath, bool weak) {
	String path;
	if (!filepath::isAbsolute(ipath.get())) {
		path = filesystem::currentDir<Interface>(ipath.get());
	} else {
		path = ipath.get().str<Interface>();
	}
	auto file = filesystem::openForReading(path);
	if (!file) {
		return false;
	}

	_filePath = path;
	if (!readStruct([&] (uint8_t *buf, size_t size) {
		return file.read(buf, size) == size;
	})) {
		_filePath.clear();
		return false;
	}

	if (_fileStruct.blocks[toInt(BlockElementType::String)].eltCount > 0) {
		if (!ObjStorageFile_readBlock<char>(file, _fileStruct.blocks[toInt(BlockElementType::String)], [this] (size_t size) {
			_strings.resize(size);
			return _strings.data();
		})) {
			_strings.clear();
		}
	}

	if (!ObjStorageFile_readBlock<Object>(file, _fileStruct.blocks[toInt(BlockElementType::Object)], [this] (size_t size) {
		_objects.resize(size);
		return _objects.data();
	})) {
		_objects.clear();
		return false;
	}

	if (!weak) {
		if (!ObjStorageFile_readBlock<Vertex>(file, _fileStruct.blocks[toInt(BlockElementType::Vertex)], [this] (size_t size) {
			_vertexes.resize(size);
			return _vertexes.data();
		})) {
			_vertexes.clear();
			return false;
		}

		if (!ObjStorageFile_readBlock<Index>(file, _fileStruct.blocks[toInt(BlockElementType::Index)], [this] (size_t size) {
			_indexes.resize(size);
			return _indexes.data();
		})) {
			_indexes.clear();
			return false;
		}
	}

	return true;
}

template <typename T, typename Callback>
static bool ObjStorageFile_readBlock(BytesView data, const ObjBundleFileBase::BlockHeader &block, const Callback &cb) {
	if (block.fileSize > 0 && block.eltCount > 0) {
		if ((block.flags & ObjBundleFileBase::BlockFlags::Compressed) != ObjBundleFileBase::BlockFlags::None) {
			if (auto s = data::getDecompressedSize(data.data() + block.fileOffset, block.fileSize)) {
				if (s / sizeof(T) == block.eltCount) {
					auto d = cb(block.eltCount);
					return data::decompress(data.data() + block.fileOffset, block.fileSize, (uint8_t *)d, s) == s;
				} else {
					return false;
				}
			}
		}

		if ( block.fileSize / sizeof(T) == block.eltCount) {
			auto d = cb(block.eltCount);
			memcpy((uint8_t *)d, data.data() + block.fileOffset, block.fileSize);
			return true;
		}
	}

	return false;
}

bool ObjBundleFileBase::readFile(BytesView data) {
	if (data.size() < sizeof(FileStruct)) {
		return false;
	}

	_filePath = "<memory>";

	CoderSource source(data);
	if (!readStruct([&] (uint8_t *buf, size_t size) {
		return source.read(buf, size) == size;
	})) {
		return false;
	}

	if (!ObjStorageFile_readBlock<char>(data, _fileStruct.blocks[toInt(BlockElementType::String)], [this] (size_t size) {
		_strings.resize(size);
		return _strings.data();
	})) {
		_strings.clear();
	}

	if (!ObjStorageFile_readBlock<Object>(data, _fileStruct.blocks[toInt(BlockElementType::Object)], [this] (size_t size) {
		_objects.resize(size);
		return _objects.data();
	})) {
		_objects.clear();
		return false;
	}

	if (!ObjStorageFile_readBlock<Vertex>(data, _fileStruct.blocks[toInt(BlockElementType::Vertex)], [this] (size_t size) {
		_vertexes.resize(size);
		return _vertexes.data();
	})) {
		_vertexes.clear();
		return false;
	}

	if (!ObjStorageFile_readBlock<Index>(data, _fileStruct.blocks[toInt(BlockElementType::Index)], [this] (size_t size) {
		_indexes.resize(size);
		return _indexes.data();
	})) {
		_indexes.clear();
		return false;
	}

	return true;
}

bool ObjBundleFileBase::readStruct(const Callback<bool(uint8_t *buf, size_t bytesCount)> &readCallback) {
	FileStruct fstruct;
	if (!readCallback((uint8_t *)&fstruct.header, sizeof(FileHeader))) {
		log::vtext("ObjStorageFile", "Fail read file, fail to read header: ", _filePath);
		return false;
	}

	if (memcmp(fstruct.header.header, Signature, 8) != 0) {
		log::vtext("ObjStorageFile", "Fail read file, invalid signature: ", _filePath);
		return false;
	}

	if (fstruct.header.nblocks <= 0) {
		log::vtext("ObjStorageFile", "Fail read file, invalid blocks count: ", _filePath);
		return false;
	}

	Vector<BlockHeader> blocks; blocks.resize(fstruct.header.nblocks);

	if (!readCallback((uint8_t *)blocks.data(), sizeof(BlockHeader) * fstruct.header.nblocks)) {
		log::vtext("ObjStorageFile", "Fail read file, fail to read blocks headers: ", _filePath);
		return false;
	}

	for (auto &it : blocks) {
		if (it.fileOffset + it.fileSize > fstruct.header.fileSize) {
			log::vtext("ObjStorageFile", "Invalid block position in file: ", _filePath);
			return false;
		}
		switch (it.type) {
		case BlockElementType::Object:
		case BlockElementType::String:
		case BlockElementType::Vertex:
		case BlockElementType::Index:
			fstruct.blocks[toInt(it.type)] = it;
			break;
		default:
			break;
		}
	}

	if (fstruct.blocks[0].fileOffset != 0 && fstruct.blocks[2].fileOffset != 0 && fstruct.blocks[3].fileOffset != 0) {
		_fileStruct = fstruct;
		return true;
	}

	log::vtext("ObjStorageFile", "Fail read file, objects data, vertexes or indexes not found: ", _filePath);
	return false;
}

void ObjBundleFileBase::setup(FileStruct &fstruct) const {
	memcpy(fstruct.header.header, ObjBundleFileBase::Signature, 8 * sizeof(uint8_t));
	fstruct.header.nblocks = 4;
	fstruct.header.reserved = 0;

	uint64_t targetFileOffset = sizeof(fstruct.header) + sizeof(ObjBundleFileBase::BlockHeader) * 4;

	fstruct.blocks[0].type = ObjBundleFileBase::BlockElementType::Object;
	fstruct.blocks[0].flags = BlockFlags::None;
	fstruct.blocks[0].eltSize = sizeof(ObjBundleFileBase::Object);
	fstruct.blocks[0].eltCount = _objects.size();
	fstruct.blocks[0].fileSize = fstruct.blocks[0].eltCount * fstruct.blocks[0].eltSize;
	fstruct.blocks[0].fileOffset = targetFileOffset;

	targetFileOffset += fstruct.blocks[0].fileSize;

	fstruct.blocks[1].type = ObjBundleFileBase::BlockElementType::String;
	fstruct.blocks[1].flags = BlockFlags::None;
	fstruct.blocks[1].eltSize = sizeof(char);
	fstruct.blocks[1].eltCount = _strings.size();
	fstruct.blocks[1].fileSize = fstruct.blocks[1].eltCount * fstruct.blocks[1].eltSize;
	fstruct.blocks[1].fileOffset = targetFileOffset;

	targetFileOffset += fstruct.blocks[1].fileSize;

	fstruct.blocks[2].type = ObjBundleFileBase::BlockElementType::Vertex;
	fstruct.blocks[2].flags = BlockFlags::None;
	fstruct.blocks[2].eltSize = sizeof(ObjBundleFileBase::Vertex);
	fstruct.blocks[2].eltCount = _vertexes.size();
	fstruct.blocks[2].fileSize = fstruct.blocks[2].eltCount * fstruct.blocks[2].eltSize;
	fstruct.blocks[2].fileOffset = targetFileOffset;

	targetFileOffset += fstruct.blocks[2].fileSize;

	fstruct.blocks[3].type = ObjBundleFileBase::BlockElementType::Index;
	fstruct.blocks[3].flags = BlockFlags::None;
	fstruct.blocks[3].eltSize = sizeof(ObjBundleFileBase::Index);
	fstruct.blocks[3].eltCount = _indexes.size();
	fstruct.blocks[3].fileSize = fstruct.blocks[3].eltCount * fstruct.blocks[3].eltSize;
	fstruct.blocks[3].fileOffset = targetFileOffset;

	targetFileOffset += fstruct.blocks[3].fileSize;

	fstruct.header.fileSize = targetFileOffset;
}

const ObjBundleFileBase::Vertex *ObjBundleFileBase::findVertex(const Vertex &vertex) const {
	for (auto &it : _vertexes) {
		if (it == vertex) {
			return &it;
		}
	}
	return nullptr;
}

}
