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

#ifndef XENOLITH_CORE_DIRECTOR_XLRESOURCECACHE_H_
#define XENOLITH_CORE_DIRECTOR_XLRESOURCECACHE_H_

#include "XLDefine.h"
#include "XLGlMaterial.h"
#include "XLTexture.h"
#include "XLMeshIndex.h"
#include "XLTemporaryResource.h"

namespace stappler::xenolith {

class ResourceCache : public Ref {
public:
	static Rc<ResourceCache> getInstance();

	virtual ~ResourceCache();

	bool init();
	void invalidate();

	void update(Director *, const UpdateTime &);

	void addImage(gl::ImageData &&);
	void addResource(const Rc<renderqueue::Resource> &);
	void removeResource(StringView);

	Rc<Texture> acquireTexture(StringView) const;
	Rc<MeshIndex> acquireMeshIndex(StringView) const;

	const gl::ImageData *getEmptyImage() const;
	const gl::ImageData *getSolidImage() const;

	/*const gl::BufferData * addExternalBufferByRef(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, FilePath data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, BytesView data);
	const gl::BufferData * addExternalBuffer(StringView key, gl::BufferInfo &&, size_t,
			const memory::function<void(const gl::BufferData::DataCallback &)> &cb);*/

	Rc<Texture> addExternalImageByRef(StringView key, gl::ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, gl::ImageInfo &&, FilePath data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, gl::ImageInfo &&, BytesView data,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);
	Rc<Texture> addExternalImage(StringView key, gl::ImageInfo &&,
			const memory::function<void(uint8_t *, uint64_t, const gl::ImageData::DataCallback &)> &cb,
			TimeInterval = TimeInterval(), TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	Rc<TemporaryResource> addTemporaryResource(Rc<renderqueue::Resource> &&, TimeInterval = TimeInterval(),
			TemporaryResourceFlags flags = TemporaryResourceFlags::None);

	Rc<TemporaryResource> getTemporaryResource(StringView str) const;
	bool hasTemporaryResource(StringView) const;
	void removeTemporaryResource(StringView);

protected:
	void compileResource(Director *, TemporaryResource *);
	bool clearResource(Director *, TemporaryResource *);

	Map<StringView, gl::ImageData> _images;
	Map<StringView, Rc<renderqueue::Resource>> _resources;
	Map<StringView, Rc<TemporaryResource>> _temporaries;
};

}

#endif /* XENOLITH_CORE_DIRECTOR_XLRESOURCECACHE_H_ */
