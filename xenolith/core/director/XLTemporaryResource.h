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

#ifndef XENOLITH_CORE_DIRECTOR_XLTEMPORARYRESOURCE_H_
#define XENOLITH_CORE_DIRECTOR_XLTEMPORARYRESOURCE_H_

#include "XLDefine.h"
#include "XLRenderQueueResource.h"

namespace stappler::xenolith {

class Scene;
class ResourceObject;
class Texture;

enum class TemporaryResourceFlags {
	None = 0,
	Loaded = 1 << 0,
	RemoveOnClear = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(TemporaryResourceFlags)

class TemporaryResource : public Ref {
public:
	static EventHeader onLoaded; // bool - true если ресурс загружен, false если выгружен

	virtual ~TemporaryResource();

	bool init(Rc<renderqueue::Resource> &&, TimeInterval timeout, TemporaryResourceFlags flags);
	void invalidate();

	Rc<Texture> acquireTexture(StringView);
	Rc<MeshIndex> acquireMeshIndex(StringView);

	void setLoaded(bool);
	void setRequested(bool);
	void setTimeout(TimeInterval);

	// Загружает ресурс в память, вызывает функцию по завершению со значением true
	// Если ресурс уже загружен, вызывает функцию немедленно со значением false
	// Возвращает true если загрузка начата и false есть ресурс уже загружен
	bool load(Ref *, Function<void(bool)> &&);

	void onEnter(Scene *, ResourceObject *);
	void onExit(Scene *, ResourceObject *);

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
	Map<const gl::BufferData *, Rc<MeshIndex>> _meshIndexes;
	Set<Rc<Scene>> _scenes;
	Vector<Pair<Rc<Ref>, Function<void(bool)>>> _callbacks;
};

}

#endif /* XENOLITH_CORE_DIRECTOR_XLTEMPORARYRESOURCE_H_ */
