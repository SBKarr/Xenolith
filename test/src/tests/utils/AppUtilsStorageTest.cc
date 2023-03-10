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
#include "SPValid.h"

namespace stappler::xenolith::app {

class UtilsStorageTestComponent : public storage::Component {
public:
	static constexpr auto DbPasswordSalt = "UtilsStorageTestComponent";

	virtual ~UtilsStorageTestComponent() { }

	UtilsStorageTestComponent(storage::ComponentLoader &loader)
	: Component(loader, "UtilsStorageTest") {
		using namespace db;

		loader.exportScheme(_users.define({
			Field::Text("name", MinLength(2), MaxLength(32), Transform::Identifier, Flags::Indexed),
			Field::Password("password", MinLength(2), MaxLength(32), PasswordSalt(DbPasswordSalt))
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

	bool getAll(Function<void(Value &&)> &&, Ref *ref);
	bool createUser(StringView name, StringView password, Function<void(Value &&)> &&, Ref * = nullptr);
	bool checkUser(StringView name, StringView password, Function<void(Value &&)> &&, Ref * = nullptr);

protected:
	using storage::ComponentContainer::init;

	UtilsStorageTestComponent *_component = nullptr;
};

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

bool UtilsStorageTestComponentContainer::getAll(Function<void(Value &&)> &&cb, Ref *ref) {
	return perform([this, cb = move(cb), ref] (const storage::Server &serv, const db::Transaction &t) mutable {
		db::Value val;
		auto users = _component->getUsers().select(t, db::Query());
		for (auto &it : users.asArray()) {
			val.addString(it.getString("name"));
		}

		serv.getApplication()->performOnMainThread([cb = move(cb), val = Value(val)] () mutable {
			cb(move(val));
		}, ref);

		return true;
	}, ref);
}

bool UtilsStorageTestComponentContainer::createUser(StringView name, StringView password, Function<void(Value &&)> &&cb, Ref *ref) {
	return perform([this, cb = move(cb), name = name.str<Interface>(), password = password.str<Interface>(), ref] (const storage::Server &serv, const db::Transaction &t) mutable {
		db::Value val;
		auto u = _component->getUsers().select(t, db::Query().select("name", db::Value(name))).getValue(0);
		if (u) {
			val = _component->getUsers().update(t, u, db::Value({
				pair("password", db::Value(password)),
			}));
		} else {
			val = _component->getUsers().create(t, db::Value({
				pair("name", db::Value(name)),
				pair("password", db::Value(password)),
			}));
		}

		serv.getApplication()->performOnMainThread([cb = move(cb), val = Value(val)] () mutable {
			cb(move(val));
		}, ref);

		return true;
	}, ref);
}

bool UtilsStorageTestComponentContainer::checkUser(StringView name, StringView password, Function<void(Value &&)> &&cb, Ref *ref) {
	return perform([this, cb = move(cb), name = name.str<Interface>(), password = password.str<Interface>(), ref] (const storage::Server &serv, const db::Transaction &t) mutable {
		db::Value val;
		auto u = _component->getUsers().select(t, db::Query().select("name", db::Value(name))).getValue(0);
		if (u) {
			if (!valid::validatePassord(password, u.getBytes("password"), UtilsStorageTestComponent::DbPasswordSalt)) {
				val = db::Value("invalid_password");
			} else {
				val = move(u);
			}
		}

		serv.getApplication()->performOnMainThread([cb = move(cb), val = Value(val)] () mutable {
			cb(move(val));
		}, ref);

		return true;
	}, ref);
}

bool UtilsStorageTest::init() {
	if (!LayoutTest::init(LayoutName::UtilsStorageTest, "")) {
		return false;
	}

	_container = Rc<UtilsStorageTestComponentContainer>::create();

	_background = addChild(Rc<MaterialBackground>::create(Color::BlueGrey_500), ZOrder(1));
	_background->setAnchorPoint(Anchor::Middle);

	_inputKey = _background->addChild(Rc<material::InputField>::create(), ZOrder(1));
	_inputKey->setLabelText("Username");
	_inputKey->setAnchorPoint(Anchor::TopRight);

	_inputValue = _background->addChild(Rc<material::InputField>::create(), ZOrder(1));
	_inputValue->setLabelText("Password");
	_inputValue->setAnchorPoint(Anchor::TopLeft);

	_create = _background->addChild(Rc<material::Button>::create(material::NodeStyle::Filled));
	_create->setText("Create");
	_create->setAnchorPoint(Anchor::TopRight);
	_create->setFollowContentSize(false);
	_create->setTapCallback([this] {
		auto key = _inputKey->getInputString();
		auto value = _inputValue->getInputString();

		if (key.size() < 4 || value.size() < 4) {
			if (key.size() < 4) {
				_inputKey->setSupportingText("* should at least 4 chars");
			}
			if (value.size() < 4) {
				_inputValue->setSupportingText("* should at least 4 chars");
			}
			return;
		}
		_inputKey->setSupportingText("");
		_inputValue->setSupportingText("");

		_container->createUser(string::toUtf8<Interface>(key), string::toUtf8<Interface>(value), [this] (Value &&val) {
			StringStream out;
			out << data::EncodeFormat::Pretty << val << "\n";
			_result->setString(out.str());
		}, this);
	});

	_check = _background->addChild(Rc<material::Button>::create(material::NodeStyle::Filled));
	_check->setText("Check");
	_check->setAnchorPoint(Anchor::TopLeft);
	_check->setFollowContentSize(false);
	_check->setTapCallback([this] {
		auto key = _inputKey->getInputString();
		auto value = _inputValue->getInputString();

		if (key.size() < 4 || value.size() < 4) {
			if (key.size() < 4) {
				_inputKey->setSupportingText("* should at least 4 chars");
			}
			if (value.size() < 4) {
				_inputValue->setSupportingText("* should at least 4 chars");
			}
			return;
		}
		_inputKey->setSupportingText("");
		_inputValue->setSupportingText("");

		_container->checkUser(string::toUtf8<Interface>(key), string::toUtf8<Interface>(value), [this] (Value &&val) {
			StringStream out;
			out << data::EncodeFormat::Pretty << val << "\n";
			_result->setString(out.str());
		}, this);
	});

	_result = _background->addChild(Rc<material::TypescaleLabel>::create(material::TypescaleRole::BodyLarge));
	_result->setFontFamily("default");
	_result->setString("null");
	_result->setAnchorPoint(Anchor::MiddleTop);

	return true;
}

void UtilsStorageTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize);

	_inputKey->setContentSize(Size2(180.0f, 56.0f));
	_inputKey->setPosition(Vec2(_contentSize.width / 2.0f - 8.0f, _contentSize.height - 64.0f));

	_inputValue->setContentSize(Size2(180.0f, 56.0f));
	_inputValue->setPosition(Vec2(_contentSize.width / 2.0f + 8.0f, _contentSize.height - 64.0f));

	_create->setContentSize(Size2(120.0f, 32.0f));
	_create->setPosition(Vec2(_contentSize.width / 2.0f - 8.0f, _contentSize.height - 148.0f));

	_check->setContentSize(Size2(120.0f, 32.0f));
	_check->setPosition(Vec2(_contentSize.width / 2.0f + 8.0f, _contentSize.height - 148.0f));

	_result->setWidth(180.0f * 2.0f + 16.0f);
	_result->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 200.0f));
}

void UtilsStorageTest::onEnter(Scene *scene) {
	LayoutTest::onEnter(scene);

	auto &serv = _director->getApplication()->getStorageServer();

	serv->addComponentContainer(_container);

	_container->getAll([this] (Value &&val) {
		StringStream out;
		out << data::EncodeFormat::Pretty << val << "\n";
		_result->setString(out.str());
	}, this);
}

void UtilsStorageTest::onExit() {
	auto &serv = _director->getApplication()->getStorageServer();

	serv->removeComponentContainer(_container);

	LayoutTest::onExit();
}

}
