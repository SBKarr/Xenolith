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
#include "XLGlResource.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith {

class Texture : public NamedRef {
public:
	virtual ~Texture();

	bool init(const gl::ImageData *, const Rc<gl::Resource> &);
	bool init(const Rc<gl::DynamicImage> &);

	virtual StringView getName() const;
	gl::MaterialImage getMaterialImage() const;

	uint64_t getIndex() const;

	bool hasAlpha() const;

	Extent3 getExtent() const;

protected:
	const gl::ImageData *_data = nullptr;
	Rc<gl::Resource> _resource;
	Rc<gl::DynamicImage> _dynamic;
};

class ResourceCache : public Ref {
public:
	static Rc<ResourceCache> getInstance();

	virtual ~ResourceCache();

	bool init(gl::Device &);
	void invalidate(gl::Device &);

	void addResource(const Rc<gl::Resource> &);
	void removeResource(StringView);

	Rc<Texture> acquireTexture(StringView) const;

	const gl::ImageData *getEmptyImage() const { return &_emptyImage; }
	const gl::ImageData *getSolidImage() const { return &_solidImage; }

protected:
	gl::ImageData _emptyImage;
	gl::ImageData _solidImage;
	Map<StringView, Rc<gl::Resource>> _resources;
};

}

#endif /* XENOLITH_CORE_DIRECTOR_XLRESOURCECACHE_H_ */
