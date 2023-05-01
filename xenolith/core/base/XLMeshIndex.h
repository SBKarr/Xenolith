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

#ifndef XENOLITH_CORE_BASE_XLMESHINDEX_H_
#define XENOLITH_CORE_BASE_XLMESHINDEX_H_

#include "XLResourceObject.h"

namespace stappler::xenolith {

class MeshIndex : public ResourceObject {
public:
	virtual ~MeshIndex();

	virtual bool init(const gl::BufferData *vertexBuffer);
	virtual bool init(const gl::BufferData *vertexBuffer, const Rc<renderqueue::Resource> &);
	virtual bool init(const gl::BufferData *vertexBuffer, const Rc<TemporaryResource> &);

	virtual StringView getName() const;

	void setBuffers(const gl::BufferData *index, const gl::BufferData *vertex);

	bool isLoaded() const;

	const gl::BufferData *getVertexData() const { return _vertexData; }
	const gl::BufferData *getIndexData() const { return _indexData; }

protected:
	String _name;
	const gl::BufferData *_vertexData = nullptr;
	const gl::BufferData *_indexData = nullptr;
};

}

#endif /* XENOLITH_CORE_BASE_XLMESHINDEX_H_ */
