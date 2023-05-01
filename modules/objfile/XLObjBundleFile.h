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

#ifndef MODULES_OBJFILE_XLOBJBUNDLEFILE_H_
#define MODULES_OBJFILE_XLOBJBUNDLEFILE_H_

//#if XENOLITH

#include "XLGl.h"
#include "XLObjBundleFileBase.h"
#include "XLMeshIndex.h"

namespace stappler::xenolith::obj {

class ObjBundleFile : public ObjBundleFileBase {
public:
	using DataCallback = memory::callback<void(BytesView)>;

	virtual ~ObjBundleFile() { }

	size_t getVertexBufferSize() const;
	size_t getIndexBufferSize() const;

	gl::BufferInfo getVertexBufferInfo() const;
	gl::BufferInfo getIndexBufferInfo() const;

	void loadVertexBuffer(uint8_t *, uint64_t, const DataCallback &);
	void loadIndexBuffer(uint8_t *, uint64_t, const DataCallback &);

	void createMeshIndexForResource(renderqueue::Resource::Builder &, StringView);

protected:
	void loadBuffer(BlockHeader &, BytesView, uint8_t *, uint64_t, const DataCallback &);
};

}

//#endif

#endif /* MODULES_OBJFILE_XLOBJBUNDLEFILE_H_ */
