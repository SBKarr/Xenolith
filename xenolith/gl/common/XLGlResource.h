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

#ifndef XENOLITH_GL_COMMON_XLGLRESOURCE_H_
#define XENOLITH_GL_COMMON_XLGLRESOURCE_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

class RenderPass;
class FrameHandle;

class Resource : public NamedRef {
public:
	class Builder;

	Resource();
	virtual ~Resource();

	virtual bool init(Builder &&);

	void clear();

	bool isCompiled() const;
	void setCompiled(bool);

	const RenderQueue *getOwner() const;
	void setOwner(const RenderQueue *);

	virtual StringView getName() const override;
	memory::pool_t *getPool() const;

	const HashTable<BufferData *> &getBuffers() const;
	const HashTable<ImageData *> &getImages() const;

	const BufferData *getBuffer(StringView) const;
	const ImageData *getImage(StringView) const;

protected:
	friend class ResourceCache;

	struct ResourceData;

	ResourceData *_data = nullptr;
};

class Resource::Builder final {
public:
	Builder(StringView);
	~Builder();

	const BufferData * addBufferByRef(StringView key, RenderPass *, BufferInfo &&, BytesView data);
	const BufferData * addBuffer(StringView key, RenderPass *, BufferInfo &&, FilePath data);
	const BufferData * addBuffer(StringView key, RenderPass *, BufferInfo &&, BytesView data);
	const BufferData * addBuffer(StringView key, RenderPass *, BufferInfo &&, size_t,
			const memory::function<void(const BufferData::DataCallback &)> &cb);

	const ImageData * addImageByRef(StringView key, RenderPass *, ImageInfo &&, BytesView data);
	const ImageData * addImage(StringView key, RenderPass *, ImageInfo &&img, FilePath data);
	const ImageData * addImage(StringView key, RenderPass *, ImageInfo &&img, BytesView data);
	const ImageData * addImage(StringView key, RenderPass *, ImageInfo &&img,
			const memory::function<void(const ImageData::DataCallback &)> &cb);

protected:
	friend class Resource;

	ResourceData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRESOURCE_H_ */
