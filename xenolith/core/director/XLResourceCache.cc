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

bool Texture::init(const gl::ImageData *data, const Rc<gl::Resource> &res) {
	_data = data;
	if (res) {
		_resource = res;
	}
	return true;
}

StringView Texture::getName() const {
	return _data->key;
}

const gl::ImageObject *Texture::getImage() const {
	if (_data->image) {
		return _data->image.get();
	}
	return nullptr;
}

uint64_t Texture::getIndex() const {
	if (_data->image) {
		return _data->image->getIndex();
	}
	return 0;
}

Rc<ResourceCache> ResourceCache::getInstance() {
	if (auto app = Application::getInstance()) {
		return app->getResourceCache();
	}
	return nullptr;
}

bool ResourceCache::init(gl::Device &dev) {
	_emptyImage = dev.getEmptyImage();
	_solidImage = dev.getSolidImage();
	return true;
}

void ResourceCache::invalidate(gl::Device &dev) {
	_emptyImage.image = nullptr;
	_solidImage.image = nullptr;
}

void ResourceCache::addResource(const Rc<gl::Resource> &req) {
	_resources.emplace(req->getName(), req).first->second;
}

void ResourceCache::removeResource(StringView requestName) {
	_resources.erase(requestName);
}

Rc<Texture> ResourceCache::acquireTexture(StringView str) const {
	if (str == EmptyTextureName) {
		return Rc<Texture>::create(&_emptyImage, nullptr);
	} else if (str == SolidTextureName) {
		return Rc<Texture>::create(&_solidImage, nullptr);
	} else {
		for (auto &it : _resources) {
			if (auto v = it.second->getImage(str)) {
				return Rc<Texture>::create(v, it.second);
			}
		}
	}
	log::vtext("ResourceCache", "Texture not found: ", str);
	return nullptr;
}

}
