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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERESOURCE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERESOURCE_H_

#include "XLGl.h"
#include "XLRenderQueue.h"

namespace stappler::xenolith::renderqueue {

class Resource : public NamedRef {
public:
	class Builder;

	static void loadImageFileData(StringView path, gl::ImageFormat fmt, const gl::ImageData::DataCallback &dcb);

	Resource();
	virtual ~Resource();

	virtual bool init(Builder &&);

	void clear();

	bool isCompiled() const;
	void setCompiled(bool);

	const Queue *getOwner() const;
	void setOwner(const Queue *);

	virtual StringView getName() const override;
	memory::pool_t *getPool() const;

	const HashTable<gl::BufferData *> &getBuffers() const;
	const HashTable<gl::ImageData *> &getImages() const;

	const gl::BufferData *getBuffer(StringView) const;
	const gl::ImageData *getImage(StringView) const;

protected:
	friend class ResourceCache;

	struct ResourceData;

	ResourceData *_data = nullptr;
};

class Resource::Builder final {
public:
	Builder(StringView);
	~Builder();

	const gl::BufferData * addBufferByRef(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addBuffer(StringView key, gl::BufferInfo &&, FilePath data);
	const gl::BufferData * addBuffer(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addBuffer(StringView key, gl::BufferInfo &&, size_t,
			const memory::function<void(const gl::BufferData::DataCallback &)> &cb);

	const gl::ImageData * addImageByRef(StringView key, gl::ImageInfo &&, BytesView data);
	const gl::ImageData * addImage(StringView key, gl::ImageInfo &&img, FilePath data);
	const gl::ImageData * addImage(StringView key, gl::ImageInfo &&img, BytesView data);
	const gl::ImageData * addImage(StringView key, gl::ImageInfo &&img,
			const memory::function<void(const gl::ImageData::DataCallback &)> &cb);

protected:
	friend class Resource;

	ResourceData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUERESOURCE_H_ */
