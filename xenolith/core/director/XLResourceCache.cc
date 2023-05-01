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

#include "XLResourceCache.h"
#include "XLDirector.h"
#include "XLGlView.h"
#include "XLGlLoop.h"
#include "XLScene.h"

namespace stappler::xenolith {

Rc<ResourceCache> ResourceCache::getInstance() {
	if (auto app = Application::getInstance()) {
		return app->getResourceCache();
	}
	return nullptr;
}

ResourceCache::~ResourceCache() { }

bool ResourceCache::init() {
	return true;
}

void ResourceCache::invalidate() {
	for (auto &it : _temporaries) {
		it.second->invalidate();
	}
	_images.clear();
	_temporaries.clear();
	_resources.clear();
}

void ResourceCache::update(Director *dir, const UpdateTime &time) {
	auto it = _temporaries.begin();
	while (it != _temporaries.end()) {
		if (it->second->getUsersCount() > 0 && !it->second->isRequested()) {
			compileResource(dir, it->second);
			++ it;
		} else if (it->second->isDeprecated(time)) {
			if (clearResource(dir, it->second)) {
				it = _temporaries.erase(it);
			} else {
				++ it;
			}
		} else {
			++ it;
		}
	}
}

void ResourceCache::addImage(gl::ImageData &&data) {
	auto name = data.key;
	_images.emplace(name, move(data));
}

void ResourceCache::addResource(const Rc<gl::Resource> &req) {
	_resources.emplace(req->getName(), req);
}

void ResourceCache::removeResource(StringView requestName) {
	_resources.erase(requestName);
}

Rc<Texture> ResourceCache::acquireTexture(StringView str) const {
	auto iit = _images.find(str);
	if (iit != _images.end()) {
		return Rc<Texture>::create(&iit->second);
	}

	for (auto &it : _temporaries) {
		if (auto tex = it.second->acquireTexture(str)) {
			return tex;
		}
	}

	for (auto &it : _resources) {
		if (auto v = it.second->getImage(str)) {
			return Rc<Texture>::create(v, it.second);
		}
	}

	log::vtext("ResourceCache", "Texture not found: ", str);
	return nullptr;
}

Rc<MeshIndex> ResourceCache::acquireMeshIndex(StringView str) const {
	for (auto &it : _temporaries) {
		if (auto mesh = it.second->acquireMeshIndex(str)) {
			return mesh;
		}
	}

	for (auto &it : _resources) {
		if (auto v = it.second->getBuffer(str)) {
			return Rc<MeshIndex>::create(v, it.second);
		}
	}

	log::vtext("ResourceCache", "MeshIndex not found: ", str);
	return nullptr;
}

const gl::ImageData *ResourceCache::getEmptyImage() const {
	auto iit = _images.find(StringView(EmptyTextureName));
	if (iit != _images.end()) {
		return &iit->second;
	}
	return nullptr;
}

const gl::ImageData *ResourceCache::getSolidImage() const {
	auto iit = _images.find(StringView(SolidTextureName));
	if (iit != _images.end()) {
		return &iit->second;
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImageByRef(StringView key, gl::ImageInfo &&info, BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::vtext("ResourceCache", "Resource '", key, "' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImageByRef(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival, flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info, FilePath data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::vtext("ResourceCache", "Resource '", key, "' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival, flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info, BytesView data, TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::vtext("ResourceCache", "Resource '", key, "' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival, flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}

	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info,
		const memory::function<void(uint8_t *, uint64_t, const gl::ImageData::DataCallback &)> &cb,
		TimeInterval ival, TemporaryResourceFlags flags) {
	auto it = _temporaries.find(key);
	if (it != _temporaries.end()) {
		if (auto tex = it->second->acquireTexture(key)) {
			return tex;
		}
		log::vtext("ResourceCache", "Resource '", key, "' already exists, but no texture '", key, "' found");
		return nullptr;
	}

	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), cb)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival, flags)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<TemporaryResource> ResourceCache::addTemporaryResource(Rc<renderqueue::Resource> &&res, TimeInterval ival, TemporaryResourceFlags flags) {
	auto tmp = Rc<TemporaryResource>::create(move(res), ival, flags);
	auto it = _temporaries.find(tmp->getName());
	if (it != _temporaries.end()) {
		_temporaries.erase(it);
	}
	it = _temporaries.emplace(tmp->getName(), move(tmp)).first;
	return it->second;
}

Rc<TemporaryResource> ResourceCache::getTemporaryResource(StringView str) const {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		return it->second;
	}
	return nullptr;
}

bool ResourceCache::hasTemporaryResource(StringView str) const {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		return true;
	}
	return false;
}

void ResourceCache::removeTemporaryResource(StringView str) {
	auto it = _temporaries.find(str);
	if (it != _temporaries.end()) {
		it->second->clear();
		_temporaries.erase(it);
	}
}

void ResourceCache::compileResource(Director *dir, TemporaryResource *res) {
	res->setRequested(true);
	dir->getView()->getLoop()->compileResource(Rc<renderqueue::Resource>(res->getResource()),
			[res = Rc<TemporaryResource>(res)] (bool success) mutable {
		Application::getInstance()->performOnMainThread([res = move(res), success] {
			res->setLoaded(success);
		}, nullptr, false);
	});
}

bool ResourceCache::clearResource(Director *dir, TemporaryResource *res) {
	return res->clear();
}

}
