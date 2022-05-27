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

#ifndef XENOLITH_FEATURES_STORAGE_XLSTORAGESERVER_H_
#define XENOLITH_FEATURES_STORAGE_XLSTORAGESERVER_H_

#include "XLDefine.h"
#include "XLEventHeader.h"
#include "STStorageScheme.h"

namespace stappler::xenolith::storage {

class Server;

class ServerComponent : public db::mem::AllocBase {
public:
	using Scheme = db::Scheme;

	virtual ~ServerComponent() { }

	ServerComponent(StringView name);

	virtual void onChildInit(Server &);
	virtual void onStorageInit(Server &, const db::Adapter &);
	virtual void onStorageDisposed(Server &, const db::Adapter &);

	virtual void onComponentLoaded();
	virtual void onComponentDisposed();

	virtual void onStorageTransaction(db::Transaction &);
	virtual void onHeartbeat(Server &);

	StringView getName() const { return _name; }

	const Scheme * exportScheme(const Scheme &);

	template <typename ... Args>
	void define(Scheme &scheme, Args && ... args) {
		exportScheme(scheme);
		scheme.define(std::forward<Args>(args)...);
	}

	const Map<StringView, const Scheme *> &getSchemes() const { return _schemes; }
	const Server *getServer() const { return _server; }

protected:
	db::mem::String _name;
	Map<StringView, const Scheme *> _schemes;
	const Server *_server = nullptr;
};

class Server : public Ref {
public:
	using DataCallback = Function<void(const data::Value &)>;
	using QueryCallback = Function<void(db::Query &)>;

	class Builder;

	struct ServerData;

	using Scheme = db::Scheme;

	virtual ~Server();

	virtual bool init(Application *, const data::Value &params, const Callback<bool(Builder &)> &);

	// get value for key
	bool get(CoderSource, DataCallback &&);

	// set value for key, optionally returns previous
	bool set(CoderSource, data::Value &&, DataCallback && = nullptr);

	// remove value for key, optionally returns previous
	bool clear(CoderSource, DataCallback && = nullptr);

	bool get(const Scheme &, DataCallback &&, uint64_t oid, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const data::Value &id, db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			StringView field, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			StringView field, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const data::Value &id,
			StringView field, db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			std::initializer_list<StringView> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			std::initializer_list<StringView> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const data::Value &id,
			std::initializer_list<StringView> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			std::initializer_list<const char *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			std::initializer_list<const char *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const data::Value &id,
			std::initializer_list<const char *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;

	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			std::initializer_list<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			std::initializer_list<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, const data::Value &id,
			std::initializer_list<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;

	// returns Array with zero or more Dictionaries with object data or Null value
	bool select(const Scheme &, DataCallback &&, QueryCallback &&, db::UpdateFlags = db::UpdateFlags::None) const;

	// returns Dictionary with single object data or Null value
	bool create(const Scheme &, data::Value &&, DataCallback && = nullptr, db::UpdateFlags = db::UpdateFlags::None) const;
	bool create(const Scheme &, data::Value &&, DataCallback &&, db::Conflict::Flags) const;
	bool create(const Scheme &, data::Value &&, DataCallback &&, db::UpdateFlags, db::Conflict::Flags) const;

	bool update(const Scheme &, uint64_t oid, data::Value &&data, DataCallback && = nullptr, db::UpdateFlags = db::UpdateFlags::None) const;
	bool update(const Scheme &, const data::Value & obj, data::Value &&data, DataCallback && = nullptr, db::UpdateFlags = db::UpdateFlags::None) const;

	bool remove(const Scheme &, uint64_t oid, Function<void(bool)> && = nullptr) const;
	bool remove(const Scheme &, const data::Value &, Function<void(bool)> && = nullptr) const;

	bool count(const Scheme &, Function<void(size_t)> &&) const;
	bool count(const Scheme &, Function<void(size_t)> &&, QueryCallback &&) const;

	bool touch(const Scheme &, uint64_t id) const;
	bool touch(const Scheme &, const data::Value & obj) const;

	// perform on Server's thread
	bool perform(Function<bool(const Server &, const db::Transaction &)> &&) const;

	Application *getApplication() const;

protected:
	bool get(const Scheme &, DataCallback &&, uint64_t oid,
			Vector<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;
	bool get(const Scheme &, DataCallback &&, StringView alias,
			Vector<const db::Field *> &&fields, db::UpdateFlags = db::UpdateFlags::None) const;

	virtual void initComponents();

	ServerData *_data = nullptr;
};

class Server::Builder final {
public:
	template <typename Component>
	auto addComponent(Component *c) -> Component * {
		addComponentWithName(c->getName(), c);
		return c;
	}

	memory::pool_t *getPool() const;

protected:
	friend class Server;

	Builder(Application *, const data::Value &params);
	~Builder();

	void addComponentWithName(const StringView &name, ServerComponent *comp);

	ServerData *_data = nullptr;
};

class StorageRoot : public db::StorageRoot {
public:
	static EventHeader onBroadcast;

	virtual void scheduleAyncDbTask(const db::mem::Callback<db::mem::Function<void(const db::Transaction &)>(db::mem::pool_t *)> &setupCb) override;
	virtual db::mem::String getDocuemntRoot() const override;
	virtual const db::Scheme *getFileScheme() const override;
	virtual const db::Scheme *getUserScheme() const override;

	virtual void onLocalBroadcast(const db::mem::Value &) override;
	virtual void onStorageTransaction(db::Transaction &) override;
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLSTORAGESERVER_H_ */
