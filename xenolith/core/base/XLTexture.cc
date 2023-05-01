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

#include "XLTexture.h"
#include "XLTemporaryResource.h"

namespace stappler::xenolith {

Texture::~Texture() { }

bool Texture::init(const gl::ImageData *data) {
	if (!ResourceObject::init(ResourceType::Texture)) {
		return false;
	}

	_data = data;
	return true;
}

bool Texture::init(const gl::ImageData *data, const Rc<gl::Resource> &res) {
	if (!ResourceObject::init(ResourceType::Texture, res)) {
		return false;
	}

	_data = data;
	return true;
}

bool Texture::init(const Rc<gl::DynamicImage> &image) {
	if (!ResourceObject::init(ResourceType::Texture)) {
		return false;
	}

	_dynamic = image;
	return true;
}

bool Texture::init(const gl::ImageData *data, const Rc<TemporaryResource> &tmp) {
	if (!ResourceObject::init(ResourceType::Texture, tmp)) {
		return false;
	}

	_data = data;
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

}
