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

#include "XLTemporaryResource.h"
#include "XLApplication.h"
#include "XLTexture.h"
#include "XLScene.h"

namespace stappler::xenolith {

XL_DECLARE_EVENT_CLASS(TemporaryResource, onLoaded);

TemporaryResource::~TemporaryResource() {
	if (_resource) {
		_resource->clear();
		_resource = nullptr;
	}
}

bool TemporaryResource::init(Rc<renderqueue::Resource> &&res, TimeInterval timeout, TemporaryResourceFlags flags) {
	_atime = Application::getClockStatic();
	_timeout = timeout;
	_resource = move(res);
	_name = _resource->getName().str<Interface>();

	if ((flags & TemporaryResourceFlags::Loaded) != TemporaryResourceFlags::None) {
		setLoaded(true);
	}

	if ((flags & TemporaryResourceFlags::RemoveOnClear) != TemporaryResourceFlags::None) {
		_removeOnClear = true;
	}

	return true;
}

void TemporaryResource::invalidate() {
	for (auto &it : _textures) {
		it.second->invalidate();
	}
	for (auto &it : _meshIndexes) {
		it.second->invalidate();
	}

	_scenes.clear();
	_resource = nullptr;
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

Rc<MeshIndex> TemporaryResource::acquireMeshIndex(StringView str) {
	if (auto v = _resource->getBuffer(str)) {
		auto it = _meshIndexes.find(v);
		if (it == _meshIndexes.end()) {
			it = _meshIndexes.emplace(v, Rc<MeshIndex>::create(v, this)).first;
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

void TemporaryResource::onEnter(Scene *scene, ResourceObject *res) {
	_scenes.emplace(scene);
	_atime = Application::getClockStatic();

	if (res->getType() == ResourceType::Texture) {
		auto tex = (Texture *)res;
		auto v = tex->getImageData();
		auto it = _textures.find(v);
		if (it == _textures.end()) {
			_textures.emplace(v, tex);
		}
	} else if (res->getType() == ResourceType::MeshIndex) {
		auto mesh = (MeshIndex *)res;
		auto v = mesh->getVertexData();
		auto it = _meshIndexes.find(v);
		if (it == _meshIndexes.end()) {
			_meshIndexes.emplace(v, mesh);
		}
	}

	++ _users;
}

void TemporaryResource::onExit(Scene *, ResourceObject *) {
	_atime = Application::getClockStatic();
	-- _users;
}

bool TemporaryResource::clear() {
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
	_meshIndexes.clear();
	_scenes.clear();

	setLoaded(false);
	return _removeOnClear;
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

}
