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

#include "AppUtilsStorageTest.h"
#include "XLStorageComponent.h"
#include "XLApplication.h"

namespace stappler::xenolith::app {

class UtilsStorageTestComponent : public storage::Component {
public:
	virtual ~UtilsStorageTestComponent() { }

	UtilsStorageTestComponent(storage::ComponentLoader &loader)
	: Component(loader, "UtilsStorageTest") {
		using namespace db;

		loader.exportScheme(_users.define({
			Field::Text("name", MinLength(2), MaxLength(32), Transform::Identifier),
			Field::Password("password", MinLength(2), MaxLength(32))
		}));
	}

	virtual void handleChildInit(const storage::Server &serv, const db::Transaction &t) override {
		std::cout << "handleChildInit\n";
		Component::handleChildInit(serv, t);
	}
	virtual void handleChildRelease(const storage::Server &serv, const db::Transaction &t) override {
		std::cout << "handleChildRelease\n";
		Component::handleChildRelease(serv, t);
	}

	virtual void handleStorageTransaction(db::Transaction &t) override {
		std::cout << "handleStorageTransaction\n";
		Component::handleStorageTransaction(t);
	}
	virtual void handleHeartbeat(const storage::Server &serv) override {
		std::cout << "handleHeartbeat\n";
		Component::handleHeartbeat(serv);
	}

	const db::Scheme &getUsers() const { return _users; }

protected:
	db::Scheme _users = db::Scheme("test_users");
};

class UtilsStorageTestComponentContainer : public storage::ComponentContainer {
public:
	virtual ~UtilsStorageTestComponentContainer() { }

	virtual bool init();

	virtual void handleStorageInit(storage::ComponentLoader &loader) override;
	virtual void handleStorageDisposed(const db::Transaction &t) override;

	virtual void handleComponentsLoaded(const storage::Server &serv) override;
	virtual void handleComponentsUnloaded(const storage::Server &serv) override;

	bool createUser(StringView name, StringView password, Function<void(Value &&)> &&, Ref * = nullptr);

protected:
	using storage::ComponentContainer::init;

	UtilsStorageTestComponent *_component = nullptr;
};


bool UtilsStorageTest::init() {
	if (!LayoutTest::init(LayoutName::UtilsStorageTest, "")) {
		return false;
	}

	_container = Rc<UtilsStorageTestComponentContainer>::create();

	return true;
}

void UtilsStorageTest::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);

	auto &serv = _director->getApplication()->getStorageServer();

	serv->addComponentContainer(_container);
}

void UtilsStorageTest::onExit() {
	auto &serv = _director->getApplication()->getStorageServer();

	serv->removeComponentContainer(_container);

	LayoutTest::onExit();
}


bool UtilsStorageTestComponentContainer::init() {
	return ComponentContainer::init("UtilsStorageTest");
}

void UtilsStorageTestComponentContainer::handleStorageInit(storage::ComponentLoader &loader) {
	std::cout << "handleStorageInit\n";
	ComponentContainer::handleStorageInit(loader);
	_component = new UtilsStorageTestComponent(loader);
}
void UtilsStorageTestComponentContainer::handleStorageDisposed(const db::Transaction &t) {
	_component = nullptr;
	ComponentContainer::handleStorageDisposed(t);
	std::cout << "handleStorageDisposed\n";
}

void UtilsStorageTestComponentContainer::handleComponentsLoaded(const storage::Server &serv) {
	ComponentContainer::handleComponentsLoaded(serv);
	std::cout << "handleComponentsLoaded\n";
}
void UtilsStorageTestComponentContainer::handleComponentsUnloaded(const storage::Server &serv) {
	std::cout << "handleComponentsUnloaded\n";
	ComponentContainer::handleComponentsUnloaded(serv);
}

bool UtilsStorageTestComponentContainer::createUser(StringView name, StringView password, Function<void(Value &&)> &&cb, Ref *ref) {
	return perform([this, cb = move(cb), name = name.str<Interface>(), password = password.str<Interface>(), ref] (const storage::Server &serv, const db::Transaction &t) mutable {
		auto val = _component->getUsers().create(t, db::Value({
			pair("name", db::Value(name)),
			pair("password", db::Value(password)),
		}));

		serv.getApplication()->performOnMainThread([cb = move(cb), val = Value(val)] () mutable {
			cb(move(val));
		}, ref);

		return true;
	}, ref);
}

}
