/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLStorageComponent.h"

namespace stappler::xenolith::storage {

Component::Component(ComponentLoader &loader, StringView name)
: _name(name.str<db::Interface>()) {
	loader.exportComponent(this);
}

void Component::handleChildInit(const Server &, const db::Transaction &) {

}

void Component::handleChildRelease(const Server &, const db::Transaction &) {

}

void Component::handleStorageTransaction(db::Transaction &) { }

void Component::handleHeartbeat(const Server &) { }


bool ComponentContainer::init(StringView str) {
	_name = str.str<Interface>();
	return true;
}

void ComponentContainer::handleStorageInit(ComponentLoader &loader) {

}

void ComponentContainer::handleStorageDisposed(const db::Transaction &t) {

}

void ComponentContainer::handleComponentsLoaded(const Server &serv) {
	_loaded = true;
	_server = &serv;
}

void ComponentContainer::handleComponentsUnloaded(const Server &serv) {
	_server = nullptr;
	_loaded = false;
}

bool ComponentContainer::perform(Function<bool(const Server &, const db::Transaction &)> &&cb, Ref *ref) const {
	if (!_server || !_loaded) {
		return false;
	}

	return _server->perform(move(cb), ref);
}

}
