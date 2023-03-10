/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLStorageServer.h"
#include "XLStorageComponent.h"
#include "XLApplication.h"
#include "SPValid.h"
#include "SPThread.h"
#include "STSqlDriver.h"
#include <typeindex>

namespace stappler::xenolith::storage {

static thread_local Server::ServerData *tl_currentServer = nullptr;

struct ServerComponentData : public db::AllocBase {
	db::pool_t *pool;
	ComponentContainer *container;

	db::Map<StringView, Component *> components;
	db::Map<std::type_index, Component *> typedComponents;
	db::Map<StringView, const db::Scheme *> schemes;
};

class ServerComponentLoader : public ComponentLoader {
public:
	virtual ~ServerComponentLoader();

	ServerComponentLoader(Server::ServerData *, const db::Transaction &);

	virtual db::pool_t *getPool() const override { return _pool; }
	virtual const Server &getServer() const override { return *_server; }
	virtual const db::Transaction &getTransaction() const override { return *_transaction; }

	virtual void exportComponent(Component *) override;

	virtual const db::Scheme * exportScheme(const db::Scheme &) override;

	bool run(ComponentContainer *);

protected:
	Server::ServerData *_data = nullptr;
	db::pool_t *_pool = nullptr;
	const Server *_server = nullptr;
	const db::Transaction *_transaction = nullptr;
	ServerComponentData *_components = nullptr;
};

struct Server::ServerData : public thread::ThreadInterface<Interface> {
	struct TaskCallback {
		Function<bool(const Server &, const db::Transaction &)> callback;
		Rc<Ref> ref;

		TaskCallback() = default;
		TaskCallback(Function<bool(const Server &, const db::Transaction &)> &&cb)
		: callback(move(cb)) { }
		TaskCallback(Function<bool(const Server &, const db::Transaction &)> &&cb, Ref *ref)
		: callback(move(cb)), ref(ref) { }
	};

	memory::pool_t *serverPool = nullptr;
	memory::pool_t *threadPool = nullptr;
	Application *application = nullptr;
	db::Map<StringView, StringView> params;
	db::Map<StringView, const db::Scheme *> predefinedSchemes;
	db::Map<ComponentContainer *, ServerComponentData *> components;

	StringView serverName;
	std::thread thread;
	std::condition_variable condition;
	std::atomic_flag shouldQuit;
	Mutex mutexQueue;
	Mutex mutexFree;
	memory::PriorityQueue<TaskCallback> queue;
	db::sql::Driver *driver = nullptr;
	db::sql::Driver::Handle handle;
	Server *server = nullptr;

	db::Vector<db::Function<void(const db::Transaction &)>> *asyncTasks = nullptr;

	db::BackendInterface::Config interfaceConfig;

	// accessed from main thread only, std memory model
	mem_std::Map<StringView, Rc<ComponentContainer>> appComponents;

	const db::Transaction *currentTransaction = nullptr;

	ServerData();
	virtual ~ServerData();

	bool init();
	bool execute(const TaskCallback &);

	virtual void threadInit() override;
	virtual bool worker() override;
	virtual void threadDispose() override;

	void handleStorageTransaction(db::Transaction &);

	void addAsyncTask(const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb);

	bool addComponent(ComponentContainer *, const db::Transaction &);
	void removeComponent(ComponentContainer *, const db::Transaction &t);
};

Server::~Server() {
	if (_data) {
		for (auto &it : _data->appComponents) {
			it.second->handleComponentsUnloaded(*this);
		}
		auto serverPool = _data->serverPool;
		_data->~ServerData();
		memory::pool::destroy(serverPool);
	}
}

bool Server::init(Application *app, const Value &params) {
	auto pool = memory::pool::create();

	memory::pool::context ctx(pool);

	auto bytes = memory::pool::palloc(pool, sizeof(ServerData));

	_data = new (bytes) ServerData;
	_data->serverPool = pool;
	_data->application = app;
	_data->shouldQuit.test_and_set();

	StringView driver;

	for (auto &it : params.asDict()) {
		if (it.first == "driver") {
			driver = StringView(it.second.getString());
		} else if (it.first == "serverName") {
			_data->serverName = StringView(it.second.getString()).pdup(pool);
		} else {
			_data->params.emplace(StringView(it.first).pdup(pool), StringView(it.second.getString()).pdup(pool));
		}
	}

	if (driver.empty()) {
		driver = StringView("sqlite");
	}

	_data->driver = db::sql::Driver::open(pool, driver);
	if (!_data->driver) {
		return false;
	}

	_data->server = this;
	return _data->init();
}

Rc<ComponentContainer> Server::getComponentContainer(StringView key) const {
	auto it = _data->appComponents.find(key);
	if (it != _data->appComponents.end()) {
		return it->second;
	}
	return nullptr;
}

bool Server::addComponentContainer(const Rc<ComponentContainer> &comp) {
	if (getComponentContainer(comp->getName()) != nullptr) {
		log::vtext("storage::Server", "Component with name ", comp->getName(), " already loaded");
		return false;
	}

	perform([this, comp] (const Server &serv, const db::Transaction &t) -> bool {
		if (_data->addComponent(comp, t)) {
			_data->application->performOnMainThread([this, comp] {
				comp->handleComponentsLoaded(*this);
			}, this);
		}
		return true;
	});
	_data->appComponents.emplace(comp->getName(), comp);
	return true;
}

bool Server::removeComponentContainer(const Rc<ComponentContainer> &comp) {
	auto it = _data->appComponents.find(comp->getName());
	if (it == _data->appComponents.end()) {
		log::vtext("storage::Server", "Component with name ", comp->getName(), " is not loaded");
		return false;
	}

	if (it->second != comp) {
		log::vtext("storage::Server", "Component you try to remove is not the same that was loaded");
		return false;
	}

	perform([this, comp] (const Server &serv, const db::Transaction &t) -> bool {
		_data->removeComponent(comp, t);
		return true;
	}, comp);
	_data->appComponents.erase(it);
	comp->handleComponentsUnloaded(*this);
	return true;
}

bool Server::get(CoderSource key, DataCallback &&cb) {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, p, key = key.view().bytes<Interface>()] (const Server &serv, const db::Transaction &t) {
		auto d = t.getAdapter().get(key);
		_data->application->performOnMainThread([p, ret = Value(d)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::set(CoderSource key, Value &&data, DataCallback &&cb) {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, p, key = key.view().bytes<Interface>(), data = move(data)] (const Server &serv, const db::Transaction &t) {
			auto d = t.getAdapter().get(key);
			t.getAdapter().set(key, data);
			_data->application->performOnMainThread([p, ret = Value(d)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, key = key.view().bytes<Interface>(), data = move(data)] (const Server &serv, const db::Transaction &t) {
			t.getAdapter().set(key, data);
			return true;
		});
	}
}

bool Server::clear(CoderSource key, DataCallback &&cb) {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, p, key = key.view().bytes<Interface>()] (const Server &serv, const db::Transaction &t) {
			auto d = t.getAdapter().get(key);
			t.getAdapter().clear(key);
			_data->application->performOnMainThread([p, ret = Value(d)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, key = key.view().bytes<Interface>()] (const Server &serv, const db::Transaction &t) {
			t.getAdapter().clear(key);
			return true;
		});
	}
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
	return perform([this, scheme = &scheme, alias = alias.str<Interface>(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}
bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, db::UpdateFlags flags) const {
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

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, StringView field, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, oid, field = field.str<Interface>(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, oid, field, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, StringView field, db::UpdateFlags flags) const {
	if (!cb) {
		return false;
	}

	auto p = new DataCallback(move(cb));
	return perform([this, scheme = &scheme, alias = alias.str<Interface>(), field = field.str<Interface>(), flags, p] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, field, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, StringView field, db::UpdateFlags flags) const {
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

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, InitList<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, InitList<StringView> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, InitList<StringView> &&fields, db::UpdateFlags flags) const {
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

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, InitList<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, InitList<const char *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		if (auto f = scheme.getField(it)) {
			mem_std::emplace_ordered(fieldsVec, f);
		}
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, InitList<const char *> &&fields, db::UpdateFlags flags) const {
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

bool Server::get(const Scheme &scheme, DataCallback &&cb, uint64_t oid, InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		mem_std::emplace_ordered(fieldsVec, it);
	}
	return get(scheme, move(cb), oid, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, StringView alias, InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
	Vector<const db::Field *> fieldsVec;
	for (auto &it : fields) {
		mem_std::emplace_ordered(fieldsVec, it);
	}
	return get(scheme, move(cb), alias, move(fieldsVec), flags);
}

bool Server::get(const Scheme &scheme, DataCallback &&cb, const Value &id, InitList<const db::Field *> &&fields, db::UpdateFlags flags) const {
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

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	return create(scheme, move(data), move(cb), flags, db::Conflict::None);
}

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb, db::Conflict::Flags conflict) const {
	return create(scheme, move(data), move(cb), db::UpdateFlags::None, conflict);
}

bool Server::create(const Scheme &scheme, Value &&data, DataCallback &&cb, db::UpdateFlags flags, db::Conflict::Flags conflict) const {
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

bool Server::update(const Scheme &scheme, uint64_t oid, Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, oid, data = move(data), flags, p] (const Server &serv, const db::Transaction &t) {
			db::Value patch(data);
			auto ret = scheme->update(t, oid, patch, flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, oid, data = move(data), flags] (const Server &serv, const db::Transaction &t) {
			db::Value patch(data);
			scheme->update(t, oid, patch, flags | db::UpdateFlags::NoReturn);
			return true;
		});
	}
}

bool Server::update(const Scheme &scheme, const Value & obj, Value &&data, DataCallback &&cb, db::UpdateFlags flags) const {
	if (cb) {
		auto p = new DataCallback(move(cb));
		return perform([this, scheme = &scheme, obj, data = move(data), flags, p] (const Server &serv, const db::Transaction &t) {
			db::Value value(obj);
			db::Value patch(data);
			auto ret = scheme->update(t, value, patch, flags);
			_data->application->performOnMainThread([p, ret = move(ret)] {
				(*p)(ret);
				delete p;
			});
			return true;
		});
	} else {
		return perform([this, scheme = &scheme, obj, data = move(data), flags] (const Server &serv, const db::Transaction &t) {
			db::Value value(obj);
			db::Value patch(data);
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

bool Server::remove(const Scheme &scheme, const Value &obj, Function<void(bool)> &&cb) const {
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

bool Server::touch(const Scheme &scheme, const Value & obj) const {
	return perform([this, scheme = &scheme, obj] (const Server &serv, const db::Transaction &t) {
		db::Value value(obj);
		scheme->touch(t, value);
		return true;
	});
}

bool Server::perform(Function<bool(const Server &, const db::Transaction &)> &&cb, Ref *ref) const {
	if (std::this_thread::get_id() == _data->thread.get_id()) {
		_data->execute(ServerData::TaskCallback(move(cb), ref));
	} else {
		_data->queue.push(0, false, ServerData::TaskCallback(move(cb), ref));
		_data->condition.notify_one();
	}
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
	return perform([this, scheme = &scheme, alias = alias.str<Interface>(), flags, p, fields = move(fields)] (const Server &serv, const db::Transaction &t) {
		auto ret = scheme->get(t, alias, fields, flags);
		_data->application->performOnMainThread([p, ret = move(ret)] {
			(*p)(ret);
			delete p;
		});
		return true;
	});
}

Server::ServerData::ServerData() {
	queue.setQueueLocking(mutexQueue);
	queue.setFreeLocking(mutexFree);
}

Server::ServerData::~ServerData() {
	shouldQuit.clear();
	condition.notify_all();
	thread.join();
}

bool Server::ServerData::init() {
	thread = std::thread(ThreadInterface::workerThread, this, nullptr);
	return true;
}

bool Server::ServerData::execute(const TaskCallback &task) {
	if (currentTransaction) {
		if (!task.callback) {
			return false;
		}
		return task.callback(*server, *currentTransaction);
	}

	bool ret = false;

	memory::pool::push(threadPool);

	driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
		adapter.performWithTransaction([&] (const db::Transaction &t) {
			currentTransaction = &t;
			auto ret = task.callback(*server, t);
			currentTransaction = nullptr;
			return ret;
		});
	});

	while (asyncTasks && driver->isValid(handle)) {
		auto tmp = asyncTasks;
		asyncTasks = nullptr;

		driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
			adapter.performWithTransaction([&] (const db::Transaction &t) {
				currentTransaction = &t;
				for (auto &it : *tmp) {
					it(t);
				}
				return true;
			});
		});
	}

	memory::pool::pop();
	memory::pool::clear(threadPool);
	return ret;
}

void Server::ServerData::threadInit() {
	tl_currentServer = this;

	memory::pool::initialize();
	memory::pool::push(serverPool);
	handle = driver->connect(params);
	if (!handle.get()) {
		StringStream out;
		for (auto &it : params) {
			out << "\n\t" << it.first << ": " << it.second;
		}
		log::vtext("StorageServer", "Fail to initialize DB with params: ", out.str());
	}
	memory::pool::pop();

	threadPool = memory::pool::create();
	memory::pool::push(threadPool);

	driver->init(handle, db::Vector<db::StringView>());

	driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
		db::Scheme::initSchemes(predefinedSchemes);
		interfaceConfig.name = adapter.getDatabaseName();
		adapter.init(interfaceConfig, predefinedSchemes);
	});

	memory::pool::pop();
	memory::pool::clear(threadPool);

	if (!serverName.empty()) {
		thread::ThreadInfo::setThreadInfo(serverName);
	}

	tl_currentServer = nullptr;
}

bool Server::ServerData::worker() {
	if (!shouldQuit.test_and_set()) {
		return false;
	}

	TaskCallback task;
	do {
		queue.pop_direct([&] (memory::PriorityQueue<TaskCallback>::PriorityType, TaskCallback &&cb) {
			task = move(cb);
		});
	} while (0);

	if (!task.callback) {
		std::unique_lock<std::mutex> lock(mutexQueue);
		if (!queue.empty(lock)) {
			return true;
		}
		condition.wait(lock);
		return true;
	}

	if (!driver->isValid(handle)) {
		return false;
	}

	execute(task);
	return true;
}

void Server::ServerData::threadDispose() {
	memory::pool::push(threadPool);

	while (!queue.empty()) {
		TaskCallback task;
		do {
			queue.pop_direct([&] (memory::PriorityQueue<TaskCallback>::PriorityType, TaskCallback &&cb) {
				task = move(cb);
			});
		} while (0);

		if (task.callback) {
			execute(task);
		}
	}

	if (driver->isValid(handle)) {
		driver->performWithStorage(handle, [&] (const db::Adapter &adapter) {
			auto it = components.begin();
			while (it != components.end()) {
				adapter.performWithTransaction([&] (const db::Transaction &t) {
					do {
						memory::pool::context ctx(it->second->pool);
						for (auto &iit : it->second->components) {
							iit.second->handleChildRelease(*server, t);
							iit.second->~Component();
						}

						it->second->container->handleStorageDisposed(t);
					} while (0);
					return true;
				});
				memory::pool::destroy(it->second->pool);
				it = components.erase(it);
			}
			components.clear();
		});
	}

	memory::pool::pop();

	memory::pool::destroy(threadPool);
	memory::pool::terminate();
}

void Server::ServerData::handleStorageTransaction(db::Transaction &t) {
	for (auto &it : components) {
		for (auto &iit : it.second->components) {
			iit.second->handleStorageTransaction(t);
		}
	}
}

void Server::ServerData::addAsyncTask(const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb) {
	if (!asyncTasks) {
		asyncTasks = new (threadPool) db::Vector<db::Function<void(const db::Transaction &)>>;
	}
	asyncTasks->emplace_back(setupCb(threadPool));
}

bool Server::ServerData::addComponent(ComponentContainer *comp, const db::Transaction &t) {
	ServerComponentLoader loader(this, t);

	memory::pool::push(loader.getPool());
	comp->handleStorageInit(loader);
	memory::pool::pop();

	return loader.run(comp);
}

void Server::ServerData::removeComponent(ComponentContainer *comp, const db::Transaction &t) {
	auto cmpIt = components.find(comp);
	if (cmpIt == components.end()) {
		return;
	}

	do {
		memory::pool::context ctx(cmpIt->second->pool);
		for (auto &it : cmpIt->second->components) {
			it.second->handleChildRelease(*server, t);
			it.second->~Component();
		}

		cmpIt->second->container->handleStorageDisposed(t);
	} while (0);

	memory::pool::destroy(cmpIt->second->pool);
	components.erase(cmpIt);
}

ServerComponentLoader::~ServerComponentLoader() {
	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

ServerComponentLoader::ServerComponentLoader(Server::ServerData *data, const db::Transaction &t)
: _data(data), _pool(memory::pool::create(data->serverPool)), _server(data->server), _transaction(&t) {
	memory::pool::context ctx(_pool);

	_components = new ServerComponentData;
	_components->pool = _pool;
}

void ServerComponentLoader::exportComponent(Component *comp) {
	memory::pool::context ctx(_pool);

	_components->components.emplace(comp->getName(), comp);
}

const db::Scheme * ServerComponentLoader::exportScheme(const db::Scheme &scheme) {
	return _components->schemes.emplace(scheme.getName(), &scheme).first->second;
}

bool ServerComponentLoader::run(ComponentContainer *comp) {
	memory::pool::context ctx(_pool);

	_components->container = comp;
	_data->components.emplace(comp, _components);

	db::Scheme::initSchemes(_components->schemes);
	_transaction->getAdapter().init(_data->interfaceConfig, _components->schemes);

	for (auto &it : _components->components) {
		it.second->handleChildInit(*_server, *_transaction);
	}

	_pool = nullptr;
	_components = nullptr;
	return true;
}

XL_DECLARE_EVENT_CLASS(StorageRoot, onBroadcast)

void StorageRoot::scheduleAyncDbTask(const db::Callback<db::Function<void(const db::Transaction &)>(db::pool_t *)> &setupCb) {
	if (tl_currentServer) {
		tl_currentServer->addAsyncTask(setupCb);
	}
}

db::String StorageRoot::getDocuemntRoot() const {
	return StringView(filesystem::writablePath<db::Interface>()).str<db::Interface>();
}

const db::Scheme *StorageRoot::getFileScheme() const {
	return nullptr;
}

const db::Scheme *StorageRoot::getUserScheme() const {
	return nullptr;
}

void StorageRoot::onLocalBroadcast(const db::Value &val) {
	onBroadcast(nullptr, Value(val));
}

void StorageRoot::onStorageTransaction(db::Transaction &t) {
	if (tl_currentServer) {
		tl_currentServer->handleStorageTransaction(t);
	}
}

}
