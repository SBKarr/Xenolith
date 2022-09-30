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

namespace stappler::xenolith {

Texture::~Texture() { }

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
		return Rc<Texture>::create(&iit->second, nullptr);
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

}
