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
#include "XLRenderQueueResource.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith {

class Scene;
class TemporaryResource;

enum class TemporaryResourceFlags {
	None = 0,
	Loaded = 1 << 0,
	RemoveOnClear = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(TemporaryResourceFlags)

class Texture : public NamedRef {
public:
	virtual ~Texture();

	bool init(const gl::ImageData *);
	bool init(const gl::ImageData *, const Rc<renderqueue::Resource> &);
	bool init(const gl::ImageData *, const Rc<TemporaryResource> &);
	bool init(const Rc<gl::DynamicImage> &);

	// invalidate texture from temporary resource
	void invalidate();

	virtual StringView getName() const;
	gl::MaterialImage getMaterialImage() const;

	uint64_t getIndex() const;

	bool hasAlpha() const;

	Extent3 getExtent() const;

	bool isLoaded() const;

	void onEnter(Scene *);
	void onExit(Scene *);

	const gl::ImageData *getImageData() const { return _data; }
	Rc<TemporaryResource> getTemporary() const { return _temporary; }

protected:
	const gl::ImageData *_data = nullptr;
	Rc<renderqueue::Resource> _resource;
	Rc<gl::DynamicImage> _dynamic;
	Rc<TemporaryResource> _temporary;
};

class TemporaryResource : public Ref {
public:
	static EventHeader onLoaded; // bool - true если ресурс загружен, false если выгружен

	virtual ~TemporaryResource();

	bool init(Rc<renderqueue::Resource> &&, TimeInterval timeout, TemporaryResourceFlags flags);
	void invalidate();

	Rc<Texture> acquireTexture(StringView);

	void setLoaded(bool);
	void setRequested(bool);
	void setTimeout(TimeInterval);

	// Загружает ресурс в память, вызывает функцию по завершению со значением true
	// Если ресурс уже загружен, вызывает функцию немедленно со значением false
	// Возвращает true если загрузка начата и false есть ресурс уже загружен
	bool load(Ref *, Function<void(bool)> &&);

	void onEnter(Scene *, Texture *);
	void onExit(Scene *, Texture *);

	bool clear();

	StringView getName() const;

	bool isRequested() const { return _requested; }
	bool isLoaded() const { return _loaded; }

	Time getAccessTime() const { return _atime; }
	TimeInterval getTimeout() const { return _timeout; }
	size_t getUsersCount() const { return _users; }

	const Rc<renderqueue::Resource> &getResource() const { return _resource; }

	bool isDeprecated(const UpdateTime &) const;

protected:
	bool _requested = false;
	bool _loaded = false;
	bool _removeOnClear = false;
	size_t _users = 0;
	uint64_t _atime;
	TimeInterval _timeout;
	String _name;
	Rc<renderqueue::Resource> _resource;
	Map<const gl::ImageData *, Rc<Texture>> _textures;
	Set<Rc<Scene>> _scenes;
	Vector<Pair<Rc<Ref>, Function<void(bool)>>> _callbacks;
};

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
