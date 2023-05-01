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

#include "XLObjBundleFile.h"
#include "SPFilesystem.h"

namespace stappler::xenolith::obj {

size_t ObjBundleFile::getVertexBufferSize() const {
	return _fileStruct.blocks[toInt(BlockElementType::Vertex)].eltCount
			* _fileStruct.blocks[toInt(BlockElementType::Vertex)].eltSize;
}

size_t ObjBundleFile::getIndexBufferSize() const {
	return _fileStruct.blocks[toInt(BlockElementType::Index)].eltCount
			* _fileStruct.blocks[toInt(BlockElementType::Index)].eltSize;
}

gl::BufferInfo ObjBundleFile::getVertexBufferInfo() const {
	return gl::BufferInfo(gl::BufferUsage::StorageBuffer, getVertexBufferSize(), gl::RenderPassType::Transfer);
}

gl::BufferInfo ObjBundleFile::getIndexBufferInfo() const {
	return gl::BufferInfo(gl::BufferUsage::IndexBuffer, getIndexBufferSize(), gl::RenderPassType::Transfer);
}

void ObjBundleFile::loadVertexBuffer(uint8_t *data, uint64_t size, const DataCallback &cb) {
	auto &h = _fileStruct.blocks[toInt(BlockElementType::Vertex)];

	loadBuffer(h, BytesView((const uint8_t *)_vertexes.data(), _vertexes.size() * sizeof(Vertex)), data, size, cb);
}

void ObjBundleFile::loadIndexBuffer(uint8_t *data, uint64_t size, const DataCallback &cb) {
	auto &h = _fileStruct.blocks[toInt(BlockElementType::Index)];

	loadBuffer(h, BytesView((const uint8_t *)_indexes.data(), _indexes.size() * sizeof(Index)), data, size, cb);
}

void ObjBundleFile::createMeshIndexForResource(renderqueue::Resource::Builder &builder, StringView name) {
	auto index = builder.addBuffer(toString(name, ":index"), getIndexBufferInfo(),
			[obj = Rc<ObjBundleFile>(this)] (uint8_t *bufferPtr, uint64_t bufferSize, const gl::BufferData::DataCallback &cb) {
		obj->loadIndexBuffer(bufferPtr, bufferSize, cb);
	});

	auto atlas = Rc<gl::DataAtlas>::create(gl::DataAtlas::MeshAtlas, _objects.size(), sizeof(gl::MeshIndexData), index);

	uint32_t i = 0;
	for (auto &it : _objects) {
		gl::MeshIndexData data{it.indexOffset, it.indexSize};
		atlas->addObject(getObjectName(it), &data);
		++ i;
	}

	builder.addBuffer(name, getVertexBufferInfo(),
			[obj = Rc<ObjBundleFile>(this)] (uint8_t *bufferPtr, uint64_t bufferSize, const gl::BufferData::DataCallback &cb) {
		obj->loadVertexBuffer(bufferPtr, bufferSize, cb);
	}, move(atlas));
}

void ObjBundleFile::loadBuffer(BlockHeader &h, BytesView view, uint8_t *data, uint64_t size, const DataCallback &cb) {
	if (data && size >= h.eltCount * h.eltSize) {
		// direct reading into GPU buffer
		if (view.empty()) {
			if (!_filePath.empty()) {
				if (auto f = filesystem::openForReading(_filePath)) {
					f.seek(h.fileOffset, io::Seek::Set);
					if ((h.flags & BlockFlags::Compressed) != BlockFlags::None) {
						// decompress block
						Bytes b; b.resize(h.fileSize);
						f.read(b.data(), h.fileSize);
						auto compressedSize = data::getDecompressedSize(b.data(), b.size());
						if (compressedSize == h.eltCount * h.eltSize) {
							data::decompress(b.data(), b.size(), data, h.eltCount * h.eltSize);
						}
					} else {
						f.read(data, h.fileSize);
					}
				}
			}
		} else {
			memcpy(data, view.data(), view.size());
		}
	} else {
		if (view.empty()) {
			if (!_filePath.empty()) {
				if (auto f = filesystem::openForReading(_filePath)) {
					f.seek(h.fileOffset, io::Seek::Set);
					Bytes b; b.resize(h.fileSize);
					f.read(b.data(), h.fileSize);
					if ((h.flags & BlockFlags::Compressed) != BlockFlags::None) {
						auto tmp = data::decompress<Interface>(b.data(), b.size());
						cb(BytesView(tmp.data(), tmp.size()));
					} else {
						cb(BytesView(b.data(), b.size()));
					}
				}
			}
		} else {
			cb(BytesView(view.data(), view.size()));
		}
	}
}

}
