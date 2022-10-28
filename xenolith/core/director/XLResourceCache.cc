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

Texture::~Texture() { }

bool Texture::init(const gl::ImageData *data) {
	_data = data;
	return true;
}

bool Texture::init(const gl::ImageData *data, const Rc<gl::Resource> &res) {
	_data = data;
	if (res) {
		_resource = res;
	}
	return true;
}

bool Texture::init(const Rc<gl::DynamicImage> &image) {
	_dynamic = image;
	return true;
}

bool Texture::init(const gl::ImageData *data, const Rc<TemporaryResource> &tmp) {
	_data = data;
	_temporary = tmp;
	return true;
}

StringView Texture::getName() const {
	if (_dynamic) {
		return _dynamic->getInfo().key;
	} else if (_data) {
		return _data->key;
	}
	return StringView();
}

gl::MaterialImage Texture::getMaterialImage() const {
	gl::MaterialImage ret;
	if (_dynamic) {
		ret.dynamic = _dynamic->getInstance();
		ret.image = &ret.dynamic->data;
	} else {
		ret.image = _data;
	}
	return ret;
}

uint64_t Texture::getIndex() const {
	if (_dynamic) {
		return _dynamic->getInstance()->data.image->getIndex();
	} else if (_data->image) {
		return _data->image->getIndex();
	}
	return 0;
}

bool Texture::hasAlpha() const {
	if (_dynamic) {
		auto info = _dynamic->getInfo();
		auto fmt = gl::getImagePixelFormat(info.format);
		switch (fmt) {
		case gl::PixelFormat::A:
		case gl::PixelFormat::IA:
		case gl::PixelFormat::RGBA:
			return (info.hints & gl::ImageHints::Opaque) == gl::ImageHints::None;
			break;
		default:
			break;
		}
		return false;
	} else {
		auto fmt = gl::getImagePixelFormat(_data->format);
		switch (fmt) {
		case gl::PixelFormat::A:
		case gl::PixelFormat::IA:
		case gl::PixelFormat::RGBA:
			return (_data->hints & gl::ImageHints::Opaque) == gl::ImageHints::None;
			break;
		default:
			break;
		}
		return false;
	}
}

Extent3 Texture::getExtent() const {
	if (_dynamic) {
		return _dynamic->getExtent();
	} else if (_data) {
		return _data->extent;
	}
	return Extent3();
}

bool Texture::isLoaded() const {
	return _dynamic || (_temporary && _temporary->isLoaded() && _data->image) || _data->image;
}

void Texture::onEnter(Scene *scene) {
	if (_temporary) {
		_temporary->onEnter(scene, this);
	}
}

void Texture::onExit(Scene *scene) {
	if (_temporary) {
		_temporary->onExit(scene, this);
	}
}

XL_DECLARE_EVENT_CLASS(TemporaryResource, onLoaded);

TemporaryResource::~TemporaryResource() {
	_resource->clear();
	_resource = nullptr;
}

bool TemporaryResource::init(Rc<renderqueue::Resource> &&res, TimeInterval timeout) {
	_atime = Application::getClockStatic();
	_timeout = timeout;
	_resource = move(res);
	return true;
}

Rc<Texture> TemporaryResource::acquireTexture(StringView str) {
	if (auto v = _resource->getImage(str)) {
		auto it = _textures.find(v);
		if (it == _textures.end()) {
			it = _textures.emplace(v, Rc<Texture>::create(v, this)).first;
		}
		return it->second;
	}
	return nullptr;
}

void TemporaryResource::setLoaded(bool val) {
	if (val) {
		_requested = true;
		for (auto &it : _callbacks) {
			it.second(true);
			-- _users;
		}
		_callbacks.clear();
		if (_loaded != val) {
			_loaded = val;
			onLoaded(this, _loaded);
		}
	} else {
		_loaded = false;
		_requested = false;
		_resource->clear();
		onLoaded(this, _loaded);
	}
	_atime = Application::getClockStatic();
}

void TemporaryResource::setRequested(bool val) {
	_requested = val;
}

void TemporaryResource::setTimeout(TimeInterval ival) {
	_timeout = ival;
}

bool TemporaryResource::load(Ref *ref, Function<void(bool)> &&cb) {
	_atime = Application::getClockStatic();
	if (_loaded) {
		if (cb) {
			cb(false);
		}
		return false;
	} else {
		_callbacks.emplace_back(pair(ref, move(cb)));
		++ _users;
		return true;
	}
}

void TemporaryResource::onEnter(Scene *scene, Texture *tex) {
	_scenes.emplace(scene);
	_atime = Application::getClockStatic();

	auto v = tex->getImageData();
	auto it = _textures.find(v);
	if (it == _textures.end()) {
		_textures.emplace(v, tex).first;
	}

	++ _users;
}

void TemporaryResource::onExit(Scene *, Texture *) {
	_atime = Application::getClockStatic();
	-- _users;
}

void TemporaryResource::clear() {
	Vector<uint64_t> ids;
	for (auto &it : _textures) {
		if (it.first->image) {
			emplace_ordered(ids, it.first->image->getIndex());
		}
	}

	if (!ids.empty()) {
		for (auto &it : _scenes) {
			it->revokeImages(ids);
		}
	}
	_textures.clear();
	_scenes.clear();

	setLoaded(false);
}

StringView TemporaryResource::getName() const {
	return _resource->getName();
}

bool TemporaryResource::isDeprecated(const UpdateTime &time) const {
	if (_users > 0 || !_loaded) {
		return false;
	}

	if (_timeout == TimeInterval()) {
		return true;
	} else if (_atime + _timeout.toMicroseconds() < time.global) {
		return true;
	}

	return false;
}

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
	_images.clear();
}

void ResourceCache::update(Director *dir, const UpdateTime &time) {
	for (auto &it : _temporaries) {
		if (it.second->getUsersCount() > 0 && !it.second->isRequested()) {
			compileResource(dir, it.second);
		} else if (it.second->isDeprecated(time)) {
			clearResource(dir, it.second);
		}
	}
}

void ResourceCache::addImage(gl::ImageData &&data) {
	auto name = data.key;
	_images.emplace(name, move(data));
}

void ResourceCache::addResource(const Rc<gl::Resource> &req) {
	_resources.emplace(req->getName(), req).first->second;
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

Rc<Texture> ResourceCache::addExternalImageByRef(StringView key, gl::ImageInfo &&info, BytesView data, TimeInterval ival) {
	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImageByRef(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info, FilePath data, TimeInterval ival) {
	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info, BytesView data, TimeInterval ival) {
	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), data)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival)) {
			return Rc<Texture>::create(d, tmp);
		}
	}

	return nullptr;
}

Rc<Texture> ResourceCache::addExternalImage(StringView key, gl::ImageInfo &&info,
		const memory::function<void(const gl::ImageData::DataCallback &)> &cb, TimeInterval ival) {
	renderqueue::Resource::Builder builder(key);
	if (auto d = builder.addImage(key, move(info), cb)) {
		if (auto tmp = addTemporaryResource(Rc<renderqueue::Resource>::create(move(builder)), ival)) {
			return Rc<Texture>::create(d, tmp);
		}
	}
	return nullptr;
}

Rc<TemporaryResource> ResourceCache::addTemporaryResource(Rc<renderqueue::Resource> &&res, TimeInterval ival) {
	auto tmp = Rc<TemporaryResource>::create(move(res), ival);
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

void ResourceCache::clearResource(Director *dir, TemporaryResource *res) {
	res->clear();
}

}
