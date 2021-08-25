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

#include "XLDirector.h"
#include "XLResourceComponent.h"
#include "XLResourceCache.h"

namespace stappler::xenolith {

bool ResourceComponent::init(Rc<gl::Resource> req) {
	_cache = Director::getInstance()->getResourceCache();
	_resource = req;
	return true;
}

void ResourceComponent::setResource(Rc<gl::Resource> req) {
	if (_running) {
		if (_resource) {
			_cache->revoke(_resource->getName());
		}
		_resource = req;
		if (_resource) {
			_cache->request(_resource);
		}
	} else {
		_resource = req;
	}
}

Rc<gl::Resource> ResourceComponent::getResource() const {
	return _resource;
}

void ResourceComponent::onEnter() {
	if (_resource) {
		_cache->request(_resource);
	}
}

void ResourceComponent::onExit() {
	if (_resource) {
		_cache->revoke(_resource->getName());
	}
}

}
