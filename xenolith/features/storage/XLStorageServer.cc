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

#include "XLStorageServer.h"
#include "STSqlDriver.h"
#include "SPValid.h"
#include <typeindex>

namespace stappler::xenolith::storage {

struct Server::ServerData : public thread::ThreadHandlerInterface {
	using TaskCallback = Function<bool(const Server &, const db::Transaction &)>;

	memory::pool_t *serverPool;
	memory::pool_t *threadPool;
	StringView name;
	Application *application;
	db::mem::Map<db::mem::String, ServerComponent *> components;
	db::mem::Map<std::type_index, ServerComponent *> typedComponents;
	db::mem::Map<StringView, const Scheme *> schemes;
	db::mem::Map<StringView, StringView> params;

	std::thread thread;
	std::condition_variable condition;
	std::atomic_flag shouldQuit;
	Mutex mutex;
	Vector<TaskCallback> queue;
	db::sql::Driver *driver;
	db::sql::Driver::Handle handle;
	Server *server;

	virtual ~ServerData();

	void dispose();
	bool init();
	bool execute(const TaskCallback &);

	virtual void threadInit() override;
	virtual bool worker() override;
};

ServerComponent::ServerComponent(StringView name)
: _name (name.str<db::mem::Interface>()) { }

void ServerComponent::onChildInit(Server &serv) {
	_server = &serv;
}
void ServerComponent::onStorageInit(Server &, const db::Adapter &) { }
void ServerComponent::onComponentLoaded() { }
void ServerComponent::onStorageTransaction(db::Transaction &) { }
void ServerComponent::onHeartbeat(Server &) { }

const db::Scheme * ServerComponent::exportScheme(const Scheme &scheme) {
	_schemes.emplace(scheme.getName(), &scheme);
	return &scheme;
}

Server::~Server() {
	if (_data) {
		_data->~ServerData();
	}
}

bool Server::init(Application *app, StringView name, const data::Value &params, const Callback<bool(Builder &)> &cb) {
	auto b = Builder(app, name, params);
	memory::pool::push(b._data->serverPool);

	auto ret = cb(b);

	memory::pool::pop();

	if (ret) {
		_data = b._data;
		b._data = nullptr;

		initComponents();

		return true;
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, oid, flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, alias = alias.str(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const data::Value &id, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, move(cb), oid, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, move(cb), oid, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, move(cb), str, flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		StringView field, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, oid, field = field.str(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, field, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		StringView field, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, alias = alias.str(), field = field.str(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, field, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const data::Value &id,
		StringView field, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, move(cb), oid, field, flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, move(cb), oid, field, flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, move(cb), str, field, flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		std::initializer_list<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		std::initializer_list<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const data::Value &id,
		std::initializer_list<StringView> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, move(cb), oid, move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, move(cb), oid, move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, move(cb), str, move(fields), flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		std::initializer_list<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		std::initializer_list<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const data::Value &id,
		std::initializer_list<const char *> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, move(cb), oid, move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, move(cb), oid, move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, move(cb), str, move(fields), flags);
		}
	}
	return false;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		std::initializer_list<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		mem_std::emplace_ordered(fieldsVec, it);
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		std::initializer_list<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		mem_std::emplace_ordered(fieldsVec, it);
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const data::Value &id,
		std::initializer_list<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (id.isDictionary()) {
		if (auto oid = id.getInteger("__oid")) {
			return get(scheme, move(cb), oid, move(fields), flags);
		}
	} else {
		if ((id.isString() && stappler::valid::validateNumber(id.getString())) || id.isInteger()) {
			if (auto oid = id.getInteger()) {
				return get(scheme, move(cb), oid, move(fields), flags);
			}
		}

		auto &str = id.getString();
		if (!str.empty()) {
			return get(scheme, move(cb), str, move(fields), flags);
		}
	}
	return false;
}

// returns Array with zero or more Dictionaries with object data or Null value
bool Server::select(const Scheme &scheme, DataCallback &&cb, QueryCallback &&qcb, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	if (qcb) {
		auto p = new DataCallback(move(cb));
		auto q = new QueryCallback(move(qcb));
		return perform([this, scheme = &scheme, p, q, flags] (const Server &serv, const db::Transaction &t) {
			db::Query query;
			(*q)(query);
			delete q;
			auto ret = scheme->select(t, query, flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, p, flags] (const Server &serv, const db::Transaction &t) {
			auto ret = scheme->select(t, db::Query(), flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	}
}

bool Server::create(const Scheme &scheme, data::Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	return create(scheme, move(data), move(cb), flags, db::Conflict::None);
}
bool Server::create(const Scheme &scheme, data::Value &&data, DataCallback &&cb, db::Conflict::Flags conflict) const {
	return create(scheme, move(data), move(cb), db::UpdateFlags::None, conflict);
}
bool Server::create(const Scheme &scheme, data::Value &&data, DataCallback &&cb, db::UpdateFlags flags, db::Conflict::Flags conflict) const {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, data = move(data), flags, conflict, p] (const Server &serv, const db::Transaction &t) {
			auto ret = scheme->create(t, data, flags | db::UpdateFlags::NoReturn, conflict);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, data = move(data), flags, conflict] (const Server &serv, const db::Transaction &t) {
			scheme->create(t, data, flags | db::UpdateFlags::NoReturn, conflict);
			return true;
		});
	}
}

bool Server::update(const Scheme &scheme, uint64_t oid, data::Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, oid, data = move(data), flags, p] (const Server &serv, const db::Transaction &t) {
			db::mem::Value patch(data);
			auto ret = scheme->update(t, oid, patch, flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, oid, data = move(data), flags] (const Server &serv, const db::Transaction &t) {
			db::mem::Value patch(data);
			scheme->update(t, oid, patch, flags | db::UpdateFlags::NoReturn);
			return true;
		});
	}
}
bool Server::update(const Scheme &scheme, const data::Value & obj, data::Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, obj, data = move(data), flags, p] (const Server &serv, const db::Transaction &t) {
			db::mem::Value value(obj);
			db::mem::Value patch(data);
			auto ret = scheme->update(t, value, patch, flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, obj, data = move(data), flags] (const Server &serv, const db::Transaction &t) {
			db::mem::Value value(obj);
			db::mem::Value patch(data);
			scheme->update(t, value, patch, flags | db::UpdateFlags::NoReturn);
			return true;
		});
	}
}

bool Server::remove(const Scheme &scheme, uint64_t oid, Function<void(bool)> &&cb) const {
	if (cb) {
		auto p = new Function<void(bool)>(move(cb));
		return perform([this, scheme = &scheme, oid, p] (const Server &serv, const db::Transaction &t) {
			auto ret = scheme->remove(t, oid);
			_data->application->performOnMainThread([p, ret] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, oid] (const Server &serv, const db::Transaction &t) {
			scheme->remove(t, oid);
			return true;
		});
	}
}
bool Server::remove(const Scheme &scheme, const data::Value &obj, Function<void(bool)> &&cb) const {
	return remove(scheme, obj.getInteger("__oid"), move(cb));
}

bool Server::count(const Scheme &scheme, Function<void(size_t)> &&cb) const {
	if (cb) {
		auto p = new Function<void(size_t)>(move(cb));
		return perform([this, scheme = &scheme, p] (const Server &serv, const db::Transaction &t) {
			auto c = scheme->count(t);
			_data->application->performOnMainThread([p, c] {
				(*p)(c);
				delete p;
			});
			return true;
		});
	}
	return false;
}
bool Server::count(const Scheme &scheme, Function<void(size_t)> &&cb, QueryCallback &&qcb) const {
	if (qcb) {
		if (cb) {
			auto p = new Function<void(size_t)>(move(cb));
			auto q = new QueryCallback(move(qcb));
			return perform([this, scheme = &scheme, p, q] (const Server &serv, const db::Transaction &t) {
				db::Query query;
				(*q)(query);
				delete q;
				auto c = scheme->count(t, query);
				_data->application->performOnMainThread([p, c] {
					(*p)(c);
					delete p;
				});
				return true;
			});
		}
	} else {
		return count(scheme, move(cb));
	}
	return false;
}

bool Server::touch(const Scheme &scheme, uint64_t id) const {
	return perform([this, scheme = &scheme, id] (const Server &serv, const db::Transaction &t) {
		scheme->touch(t, id);
		return true;
	});
}
bool Server::touch(const Scheme &scheme, const data::Value & obj) const {
	return perform([this, scheme = &scheme, obj] (const Server &serv, const db::Transaction &t) {
		db::mem::Value value(obj);
		scheme->touch(t, value);
		return true;
	});
}

bool Server::perform(Function<bool(const Server &, const db::Transaction &)> &&cb) const {
	_data->mutex.lock();
	_data->queue.emplace_back(move(cb));
	_data->mutex.unlock();
	_data->condition.notify_one();
	return true;
}

Application *Server::getApplication() const {
	return _data->application;
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid,
		Vector<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, oid, flags, p, fields = move(fields)] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, fields, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias,
		Vector<const db::Field *> &&fields, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, alias = alias.str(), flags, p, fields = move(fields)] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, fields, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

void Server::initComponents() {
	for (auto &it : _data->components) {
		it.second->onChildInit(*this);
	}

	if (_data) {
		_data->server = this;
		_data->init();
	}
}

Server::Builder::Builder(Application *app, StringView name, const data::Value &params) {
	auto pool = memory::pool::create();

	memory::pool::context ctx(pool);
	auto bytes = memory::pool::palloc(pool, sizeof(ServerData));

	_data = new (bytes) ServerData;
	_data->serverPool = pool;
	_data->name = name.pdup(pool);
	_data->application = app;
	_data->shouldQuit.clear();

	StringView driver;

	for (auto &it : params.asDict()) {
		if (it.first == "driver") {
			driver = StringView(it.second.getString());
		} else {
			_data->params.emplace(StringView(it.first).pdup(pool), StringView(it.second.getString()).pdup(pool));
		}
	}

	if (driver.empty()) {
		driver = StringView("sqlite");
	}

	_data->driver = db::sql::Driver::open(pool, driver);
}

Server::Builder::~Builder() {
	if (_data) {
		_data->~ServerData();
	}
}

memory::pool_t *Server::Builder::getPool() const {
	return _data->serverPool;
}

void Server::Builder::addComponentWithName(const StringView &name, ServerComponent *comp) {
	_data->components.emplace(name.str<db::mem::Interface>(), comp);
	_data->typedComponents.emplace(std::type_index(typeid(*comp)), comp);

	for (auto &it : comp->getSchemes()) {
		if (!_data->schemes.emplace(it.first, it.second).second) {
			log::vtext("storage::Server", "Duplicated scheme name '", it.first, "' in component '", comp->getName(), "'");
		}
	}
}

Server::ServerData::~ServerData() {
	shouldQuit.clear();
	condition.notify_all();
	thread.join();

	memory::pool::push(serverPool);
	for (auto &it : components) {
		it.second->onComponentDisposed();
	}
	memory::pool::pop();

	memory::pool::destroy(serverPool);
}

void Server::ServerData::dispose() {
	memory::pool::push(threadPool);

	if (!driver->isValid(handle)) {
		driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
			for (auto &it : components) {
				it.second->onStorageDisposed(*server, adapter);
			}
		});
	}

	memory::pool::pop();

	memory::pool::destroy(threadPool);
	memory::pool::terminate();
}

bool Server::ServerData::init() {
	thread = StdThread(ThreadHandlerInterface::workerThread, this, nullptr);
	return true;
}

bool Server::ServerData::execute(const TaskCallback &task) {
	bool ret = false;

	memory::pool::push(serverPool);
	handle = driver->connect(params);
	memory::pool::pop();

	memory::pool::push(threadPool);

	driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
		adapter.performInTransaction([&] {
			if (auto t = db::Transaction::acquire(adapter)) {
				ret = task(*server, t);
				t.release();
			}
			return ret;
		});
	});

	memory::pool::pop();
	memory::pool::clear(threadPool);
	return ret;
}

void Server::ServerData::threadInit() {
	memory::pool::initialize();
	memory::pool::push(serverPool);
	handle = driver->connect(params);
	memory::pool::pop();

	threadPool = memory::pool::create(/*alloc*/);
	memory::pool::push(threadPool);

	driver->init(handle, db::mem::Vector<db::mem::StringView>());

	driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
		adapter.init(db::Interface::Config({adapter.getDatabaseName()}), schemes);

		for (auto &it : components) {
			it.second->onStorageInit(*server, adapter);
			auto linkId = server->retain(); // preserve server
			application->performOnMainThread([comp = it.second, serv = server, linkId] {
				comp->onComponentLoaded();
				serv->release(linkId);
			});
		}
	});

	memory::pool::pop();
	memory::pool::clear(threadPool);
}

bool Server::ServerData::worker() {
	if (!shouldQuit.test_and_set()) {
		dispose();
		return false;
	}

	TaskCallback task;
	do {
		std::unique_lock<std::mutex> lock(mutex);
		if (!queue.empty()) {
			task = std::move(queue.front());
			queue.erase(queue.begin());
		}
	} while (0);

	if (!task) {
		std::unique_lock<std::mutex> lock(mutex);
		if (!queue.empty()) {
			return true;
		}
		condition.wait(lock);
		return true;
	}

	if (!driver->isValid(handle)) {
		dispose();
		return false;
	}

	execute(task);
	return true;
}

}
