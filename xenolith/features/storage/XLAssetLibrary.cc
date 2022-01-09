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

#include "XLAssetLibrary.h"
#include "XLApplication.h"
#include "XLAsset.h"
#include "STSqlHandle.h"

namespace stappler::xenolith::storage {

class AssetStorage : public ServerComponent {
public:
	static constexpr auto DtKey = "XL.AssetLibrary.dt";

	AssetStorage(AssetLibrary *, StringView name);

	virtual void onStorageInit(storage::Server &, const db::Adapter &) override;
	virtual void onComponentLoaded() override;
	virtual void onComponentDisposed() override;

	void cleanup(const db::Transaction &t);

	db::mem::Value getAsset(const db::Transaction &, StringView) const;
	db::mem::Value createAsset(const db::Transaction &, StringView, TimeInterval) const;

	void updateAssetTtl(const db::Transaction &, int64_t, TimeInterval) const;

protected:
	AssetLibrary *_library = nullptr;
	Scheme _assetsScheme = Scheme("assets");
	Scheme _downloadsScheme = Scheme("assets_downloads");
};

bool AssetStorageServer::init(AssetLibrary *lib, const data::Value &params) {
	return Server::init(lib->getApplication(), params, [&] (Builder &builder) {
		_storage = builder.addComponent(new (builder.getPool()) AssetStorage(lib, "AssetStorage"));
		return true;
	});
}

XL_DECLARE_EVENT_CLASS(AssetLibrary, onLoaded)

AssetLibrary::AssetRequest::AssetRequest(StringView utl, AssetCallback &&cb, TimeInterval ttl)\
: url(AssetLibrary::getAssetUrl(url)), callback(move(cb)), ttl(ttl) { }

String AssetLibrary::getTempPath(StringView path) {
	return toString(path, ".tmp");
}

String AssetLibrary::getAssetUrl(StringView url) {
	if (url.starts_with("%") || url.starts_with("app://") || url.starts_with("http://") || url.starts_with("https://")
			 || url.starts_with("ftp://") || url.starts_with("ftps://")) {
		return url.str();
	} else if (url.starts_with("/")) {
		return filepath::canonical(url);
	} else {
		return toString("app://", url);
	}
}

AssetLibrary::~AssetLibrary() {
	_server = nullptr;
}

bool AssetLibrary::init(Application *app, const data::Value &dbParams) {
	_application = app; // always before server initialization
	_server = Rc<AssetStorageServer>::create(this, dbParams);
	return true;
}

void AssetLibrary::onComponentLoaded() {
	for (auto &it : _downloads) {
		it.second->perform(_application, nullptr);
	}

	_loaded = true;
	acquireAssets(_tmpRequests);
	for (auto &it : _tmpMultiRequest) {
		acquireAssets(it.first, move(it.second));
	}

	_tmpRequests.clear();
	_tmpMultiRequest.clear();

	onLoaded(this);
}

void AssetLibrary::onComponentDisposed() { }

void AssetLibrary::setServerDate(const Time &serv) {
	auto dt = serv.toMicros() - Time::now().toMicros();

	_server->set(AssetStorage::DtKey, data::Value({
		pair("dt", data::Value(dt))
	}));
}

void AssetLibrary::cleanup() {
	if (!_application->isNetworkOnline()) {
		// if we had no network connection to restore assets - do not cleanup them
		return;
	}

	auto storage = _server->getStorage();

	if (!storage) {
		return;
	}

	_server->perform([storage] (const storage::Server &serv, const db::Transaction &t) {
		storage->cleanup(t);
		return true;
	});
}

bool AssetLibrary::acquireAsset(StringView iurl, AssetCallback &&cb, TimeInterval ttl) {
	if (!_loaded) {
		_tmpRequests.emplace_back(AssetRequest(iurl, move(cb), ttl));
		return true;
	}

	auto url = getAssetUrl(iurl);
	if (auto a = getLiveAsset(url)) {
		if (cb) {
			cb(a);
		}
		return true;
	}

	auto it = _callbacks.find(url);
	if (it != _callbacks.end()) {
		it->second.push_back(cb);
	} else {
		_callbacks.emplace(url, Vector<AssetCallback>({cb}));

		_server->perform([this, url = move(url), ttl] (const Server &, const db::Transaction &t) {
			Rc<Asset> asset;
			if (auto data = _server->getStorage()->getAsset(t, url)) {
				if (data.getInteger("ttl") != int64_t(ttl.toMicros())) {
					_server->getStorage()->updateAssetTtl(t, data.getInteger("__oid"), ttl);
					data.setInteger(ttl.toMicros(), "ttl");
				}

				notifyAssetCallbacks(Rc<Asset>::alloc(this, data));
			} else {
				// no asset with this url - create one
				if (auto data = _server->getStorage()->createAsset(t, url, ttl)) {
					notifyAssetCallbacks(Rc<Asset>::alloc(this, data));
				}
			}
			return true;
		});
	}

	return true;
}

bool AssetLibrary::acquireAssets(const SpanView<AssetRequest> &vec, AssetVecCallback &&icb) {
	if (!_loaded) {
		if (!icb) {
			for (auto &it : vec) {
				_tmpRequests.push_back(it);
			}
		} else {
			_tmpMultiRequest.emplace_back(vec.vec(), move(icb));
		}
		return true;
	}

	size_t assetCount = vec.size();
	auto requests = new Vector<AssetRequest>;

	Vector<Rc<Asset>> *retVec = nullptr;
	AssetVecCallback *cb = nullptr;
	if (cb) {
		retVec = new Vector<Rc<Asset>>;
		cb = new AssetVecCallback(move(icb));
	}

	for (auto &it : vec) {
		if (auto a = getLiveAsset(it.url)) {
			if (it.callback) {
				it.callback(a);
			}
			if (retVec) {
				retVec->emplace_back(a);
			}
		} else {
			auto cbit = _callbacks.find(it.url);
			if (cbit != _callbacks.end()) {
				cbit->second.emplace_back(it.callback);
				if (cb && retVec) {
					cbit->second.push_back([cb, retVec, assetCount] (Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					});
				}
			} else {
				Vector<AssetCallback> vec;
				vec.emplace_back(it.callback);
				if (cb && retVec) {
					vec.emplace_back([cb, retVec, assetCount] (Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					});
				}
				_callbacks.emplace(it.url, move(vec));
				requests->push_back(it);
			}
		}
	}

	if (requests->empty()) {
		if (cb && retVec) {
			if (retVec->size() == assetCount) {
				(*cb)(*retVec);
				delete retVec;
				delete cb;
			}
		}
		delete requests;
		return true;
	}

	_server->perform([this, requests] (const Server &, const db::Transaction &t) {
		db::mem::Vector<int64_t> ids;
		for (auto &it : *requests) {
			Rc<Asset> asset;
			if (auto data = _server->getStorage()->getAsset(t, it.url)) {
				if (!db::mem::emplace_ordered(ids, data.getInteger("__oid"))) {
					continue;
				}

				if (data.getInteger("ttl") != int64_t(it.ttl.toMicros())) {
					_server->getStorage()->updateAssetTtl(t, data.getInteger("__oid"), it.ttl);
					data.setInteger(it.ttl.toMicros(), "ttl");
				}

				notifyAssetCallbacks(Rc<Asset>::alloc(this, data));
			} else {
				if (auto data = _server->getStorage()->createAsset(t, it.url, it.ttl)) {
					notifyAssetCallbacks(Rc<Asset>::alloc(this, data));
				}
			}
		}
		return true;
	});
	return true;
}

Asset *AssetLibrary::getLiveAsset(StringView url) const {
	auto it = _assetsByUrl.find(url);
	if (it != _assetsByUrl.end()) {
		return it->second;
	}
	return nullptr;
}

Asset *AssetLibrary::getLiveAsset(int64_t id) const {
	auto it = _assetsById.find(id);
	if (it != _assetsById.end()) {
		return it->second;
	}
	return nullptr;
}


void AssetLibrary::removeAsset(Asset *asset) {
	_assetsById.erase(asset->getId());
	_assetsByUrl.erase(asset->getUrl());
}

Time AssetLibrary::getCorrectTime() const {
	return Time::microseconds(Time::now().toMicros() + _dt);
}

void AssetLibrary::notifyAssetCallbacks(Rc<Asset> &&asset) {
	_application->performOnMainThread([this, asset = move(asset)] {
		_assetsById.emplace(asset->getId(), asset);
		_assetsByUrl.emplace(asset->getUrl(), asset);

		auto it = _callbacks.find(asset->getUrl());
		if (it != _callbacks.end()) {
			for (auto &cb : it->second) {
				if (cb) {
					cb(asset);
				}
			}
			_callbacks.erase(it);
		}
	}, this);
}

AssetStorage::AssetStorage(AssetLibrary *lib, StringView name) : ServerComponent(name), _library(lib) {
	using namespace db;

	define(_assetsScheme, mem::Vector<Field>({
		Field::Integer("size"),
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("touch", Flags::AutoCTime),
		Field::Integer("ttl"),
		Field::Text("url", MaxLength(2_KiB), Transform::Url, Flags::Unique | Flags::Indexed),
		Field::Text("contentType", MaxLength(2_KiB)),
		Field::Text("etag", MaxLength(2_KiB)),
		Field::Set("states", _downloadsScheme),
		Field::Boolean("download", db::mem::Value(false)),
	}));

	define(_downloadsScheme, mem::Vector<Field>({
		Field::Text("url", MaxLength(2_KiB), Transform::Url),
		Field::Integer("ctime", Flags::AutoCTime),
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("version"),
		Field::Object("asset", _assetsScheme, RemovePolicy::Cascade),
	}));
}

void AssetStorage::onStorageInit(storage::Server &serv, const db::Adapter &a) {
	auto v = a.get(DtKey);

	_library->_dt = v.getInteger("dt");

	if (auto t = db::Transaction::acquire(a)) {
		/*_assetsScheme.foreach(t, db::Query().select("download", data::Value(true)), [&] (db::mem::Value &val) -> bool {
			if (val.getBool("download")) {
				auto tmpPath = AssetLibrary::getTempPath(val.getString("path"));

				Asset *a = new Asset(_library, val);
				a->touchWithTime(_library->getCorrectTime());

				auto d = Rc<network::AssetHandle>::create(a, tmpPath);
				_library->_downloads.emplace(a, d);
				_library->_assets.emplace(a->getId(), a);
			}

			return true;
		});

		for (auto &it : _library->_assets) {
			_assetsScheme.update(t, it.first, db::mem::Value({
				pair("touch", db::mem::Value(it.second->getTouch()))
			}));
		}*/

		cleanup(t);
		t.release();
	}
}

void AssetStorage::onComponentLoaded() {
	_library->onComponentLoaded();
}

void AssetStorage::onComponentDisposed() {
	_library->onComponentDisposed();
}

void AssetStorage::cleanup(const db::Transaction &t) {
	auto time = _library->getCorrectTime().toMicroseconds();

	if (auto iface = dynamic_cast<db::sql::SqlHandle *>(t.getAdapter().interface())) {
		iface->performSimpleSelect(toString("SELECT __oid, url FROM ", _assetsScheme.getName(),
				" WHERE download == 0 AND ttl != 0 AND (touch + ttl) < ",
				time, ";"), [&] (db::Result &res) {
			for (auto it : res) {
				auto path = filepath::absolute(it.toString(0));
				auto cache = filepath::absolute(it.toString(1));
				auto tmpPath = AssetLibrary::getTempPath(path);

				filesystem::remove(path);
				filesystem::remove(tmpPath);
				if (!cache.empty()) {
					filesystem::remove(cache, true, true);
				}
			}
		});

		iface->performSimpleQuery(toString("DELETE FROM ", _assetsScheme.getName(),
				" WHERE download == 0 AND ttl != 0 AND touch + ttl * 2 < ",
				+ time, ";"));
	}
}

db::mem::Value AssetStorage::getAsset(const db::Transaction &t, StringView url) const {
	if (auto v = _assetsScheme.select(t, db::Query().select("url", data::Value(url)))) {
		return data::Value(v.getValue(0));
	}
	return db::mem::Value();
}

db::mem::Value AssetStorage::createAsset(const db::Transaction &t, StringView url, TimeInterval ttl) const {
	return _assetsScheme.create(t, db::mem::Value({
		pair("url", db::mem::Value(url)),
		pair("ttl", db::mem::Value(ttl)),
	}));
}

void AssetStorage::updateAssetTtl(const db::Transaction &t, int64_t id, TimeInterval ttl) const {
	_assetsScheme.update(t, id, db::mem::Value({
		pair("ttl", db::mem::Value(ttl)),
	}), db::UpdateFlags::NoReturn);
}

}
