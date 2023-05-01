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

#include "XLMeshIndex.h"
#include "XLTemporaryResource.h"

namespace stappler::xenolith {

MeshIndex::~MeshIndex() { }

bool MeshIndex::init(const gl::BufferData *vertexBuffer) {
	if (!ResourceObject::init(ResourceType::MeshIndex)) {
		return false;
	}

	_vertexData = vertexBuffer;
	return true;
}

bool MeshIndex::init(const gl::BufferData *vertexBuffer, const Rc<renderqueue::Resource> &res) {
	if (!ResourceObject::init(ResourceType::MeshIndex, res)) {
		return false;
	}

	_vertexData = vertexBuffer;
	return true;
}

bool MeshIndex::init(const gl::BufferData *vertexBuffer, const Rc<TemporaryResource> &res) {
	if (!ResourceObject::init(ResourceType::MeshIndex, res)) {
		return false;
	}

	_vertexData = vertexBuffer;
	return true;
}

StringView MeshIndex::getName() const {
	return _name;
}

void MeshIndex::setBuffers(const gl::BufferData *index, const gl::BufferData *vertex) {
	_indexData = index;
	_vertexData = vertex;
}

bool MeshIndex::isLoaded() const {
	return (_temporary && _temporary->isLoaded() && _vertexData->buffer) || _vertexData->buffer;
}

}
