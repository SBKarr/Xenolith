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
#include "XLStorageComponent.h"
#include "XLStorageServer.h"
#include "STSqlHandle.h"

namespace stappler::xenolith::storage {

class AssetComponent : public Component {
public:
	static constexpr auto DtKey = "XL.AssetLibrary.dt";

	virtual ~AssetComponent() { }

	AssetComponent(AssetLibrary *, ComponentLoader &, StringView name);

	const db::Scheme &getAssets() const { return _assets; }
	const db::Scheme &getVersions() const { return _versions; }

	virtual void handleChildInit(const Server &, const db::Transaction &) override;

	void cleanup(const db::Transaction &t);

	db::Value getAsset(const db::Transaction &, StringView) const;
	db::Value createAsset(const db::Transaction &, StringView, TimeInterval) const;

	void updateAssetTtl(const db::Transaction &, int64_t, TimeInterval) const;

protected:
	AssetLibrary *_library = nullptr;
	db::Scheme _assets = db::Scheme("assets");
	db::Scheme _versions = db::Scheme("versions");
};

XL_DECLARE_EVENT_CLASS(AssetLibrary, onLoaded)

AssetComponent::AssetComponent(AssetLibrary *l, ComponentLoader &loader, StringView name)
: Component(loader, name), _library(l) {
	using namespace db;

	loader.exportScheme(_assets.define({
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("touch", Flags::AutoCTime),
		Field::Integer("ttl"),
		Field::Text("local"),
		Field::Text("url", MaxLength(2_KiB), Transform::Url, Flags::Unique | Flags::Indexed),
		Field::Set("versions", _versions),
		Field::Boolean("download", db::Value(false), Flags::Indexed),
		Field::Data("data"),
	}));

	loader.exportScheme(_versions.define({
		Field::Text("etag", MaxLength(2_KiB)),
		Field::Integer("ctime", Flags::AutoCTime),
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("size"),
		Field::Text("type"),
		Field::Boolean("complete", db::Value(false)),
		Field::Object("asset", _assets, RemovePolicy::Cascade),
	}));
}

void AssetComponent::handleChildInit(const Server &serv, const db::Transaction &t) {
	Component::handleChildInit(serv, t);

	filesystem::mkdir(filesystem::cachesPath<Interface>("assets"));

	Time time = Time::now();
	Vector<Rc<Asset>> assetsVec;

	auto assets = _assets.select(t, db::Query().select("download", db::Value(true)));
	for (auto &it : assets.asArray()) {
		auto versions = _versions.select(t, db::Query().select("asset", it.getValue("__oid")));
		it.setValue(move(versions), "versions");

		auto &asset = assetsVec.emplace_back(Rc<Asset>::alloc(_library, it));
		asset->touch(time);

		_assets.update(t, it, db::Value({
			pair("touch", db::Value(asset->getTouch().toMicros()))
		}));
	}

	cleanup(t);

	_library->getApplication()->performOnMainThread([assetsVec = move(assetsVec), lib = _library] () mutable {
		lib->handleLibraryLoaded(move(assetsVec));
	}, _library);
}

void AssetComponent::cleanup(const db::Transaction &t) {
	Time time = Time::now();
	if (auto iface = dynamic_cast<db::sql::SqlHandle *>(t.getAdapter().interface())) {
		iface->performSimpleSelect(toString("SELECT __oid, url FROM ", _assets.getName(),
				" WHERE download == 0 AND ttl != 0 AND (touch + ttl) < ",
				time, ";"), [&] (db::Result &res) {
			for (auto it : res) {
				auto path = AssetLibrary::getAssetPath(it.toInteger(0));
				filesystem::remove(path, true, true);
			}
		});

		iface->performSimpleQuery(toString("DELETE FROM ", _assets.getName(),
				" WHERE download == 0 AND ttl != 0 AND touch + ttl * 2 < ",
				+ time, ";"));
	}
}

db::Value AssetComponent::getAsset(const db::Transaction &t, StringView url) const {
	if (auto v = _assets.select(t, db::Query().select("url", db::Value(url)))) {
		return db::Value(v.getValue(0));
	}
	return db::Value();
}

db::Value AssetComponent::createAsset(const db::Transaction &t, StringView url, TimeInterval ttl) const {
	return _assets.create(t, db::Value({
		pair("url", db::Value(url)),
		pair("ttl", db::Value(ttl)),
	}));
}

void AssetComponent::updateAssetTtl(const db::Transaction &t, int64_t id, TimeInterval ttl) const {
	_assets.update(t, id, db::Value({
		pair("ttl", db::Value(ttl)),
	}), db::UpdateFlags::NoReturn);
}

String AssetLibrary::getAssetPath(int64_t id) {
	return toString(filesystem::cachesPath<Interface>("assets"), "/", id);
}

String AssetLibrary::getAssetUrl(StringView url) {
	if (url.starts_with("%") || url.starts_with("app://") || url.starts_with("http://") || url.starts_with("https://")
			 || url.starts_with("ftp://") || url.starts_with("ftps://")) {
		return url.str<Interface>();
	} else if (url.starts_with("/")) {
		return filepath::canonical<Interface>(url);
	} else {
		return toString("app://", url);
	}
}

AssetLibrary::~AssetLibrary() {
	_server = nullptr;
}

bool AssetLibrary::init(Application *app, const Value &dbParams) {
	_application = app; // always before server initialization
	_server = Rc<Server>::create(app, dbParams);
	_server->addComponentContainer(this);
	return true;
}

void AssetLibrary::update(uint64_t clock) {
	auto it = _liveAssets.begin();
	while (it != _liveAssets.end()) {
		if ((*it)->isStorageDirty()) {
			_server->perform([this, value = (*it)->encode(), id = (*it)->getId()] (const Server &, const db::Transaction &t) {
				_component->getAssets().update(t, id, db::Value(value), db::UpdateFlags::NoReturn);
				return true;
			}, this);
			(*it)->setStorageDirty(false);
		}
		if ((*it)->getReferenceCount() == 1) {
			it = _liveAssets.erase(it);
		} else {
			++ it;
		}
	}
}

void AssetLibrary::handleStorageInit(storage::ComponentLoader &loader) {
	ComponentContainer::handleStorageInit(loader);
	_component = new AssetComponent(this, loader, "AssetComponent");
}

void AssetLibrary::handleStorageDisposed(const db::Transaction &t) {
	_component = nullptr;
	ComponentContainer::handleStorageDisposed(t);
}


bool AssetLibrary::acquireAsset(StringView iurl, AssetCallback &&cb, TimeInterval ttl, Rc<Ref> &&ref) {
	if (!_loaded) {
		_tmpRequests.push_back(AssetRequest(iurl, move(cb), ttl, move(ref)));
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
		it->second.emplace_back(move(cb), move(ref));
	} else {
		_callbacks.emplace(url, Vector<Pair<AssetCallback, Rc<Ref>>>({pair(move(cb), move(ref))}));

		_server->perform([this, url = move(url), ttl] (const Server &, const db::Transaction &t) {
			Rc<Asset> asset;
			if (auto data = _component->getAsset(t, url)) {
				if (data.getInteger("ttl") != int64_t(ttl.toMicros())) {
					_component->updateAssetTtl(t, data.getInteger("__oid"), ttl);
					data.setInteger(ttl.toMicros(), "ttl");
				}

				handleAssetLoaded(Rc<Asset>::alloc(this, data));
			} else {
				if (auto data = _component->createAsset(t, url, ttl)) {
					handleAssetLoaded(Rc<Asset>::alloc(this, data));
				}
			}
			return true;
		});
	}

	return true;
}

bool AssetLibrary::acquireAssets(SpanView<AssetRequest> vec, AssetVecCallback &&icb, Rc<Ref> &&ref) {
	if (!_loaded) {
		if (!icb && !ref) {
			for (auto &it : vec) {
				_tmpRequests.emplace_back(move(it));
			}
		} else {
			_tmpMultiRequest.emplace_back(AssetMultiRequest(vec.vec<Interface>(), move(icb), move(ref)));
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
				cbit->second.emplace_back(it.callback, ref);
				if (cb && retVec) {
					cbit->second.emplace_back([cb, retVec, assetCount] (Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					}, nullptr);
				}
			} else {
				Vector<Pair<AssetCallback, Rc<Ref>>> vec;
				vec.emplace_back(it.callback, ref);
				if (cb && retVec) {
					vec.emplace_back([cb, retVec, assetCount] (Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							(*cb)(*retVec);
							delete retVec;
							delete cb;
						}
					}, nullptr);
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
		db::Vector<int64_t> ids;
		for (auto &it : *requests) {
			Rc<Asset> asset;
			if (auto data = _component->getAsset(t, it.url)) {
				if (!db::emplace_ordered(ids, data.getInteger("__oid"))) {
					continue;
				}

				if (data.getInteger("ttl") != int64_t(it.ttl.toMicros())) {
					_component->updateAssetTtl(t, data.getInteger("__oid"), it.ttl);
					data.setInteger(it.ttl.toMicros(), "ttl");
				}

				handleAssetLoaded(Rc<Asset>::alloc(this, data));
			} else {
				if (auto data = _component->createAsset(t, it.url, it.ttl)) {
					handleAssetLoaded(Rc<Asset>::alloc(this, data));
				}
			}
		}
		return true;
	});
	return true;
}

int64_t AssetLibrary::addVersion(const db::Transaction &t, int64_t assetId, const Asset::VersionData &data) {
	auto version = _component->getVersions().create(t, db::Value({
		pair("asset", db::Value(assetId)),
		pair("etag", db::Value(data.etag)),
		pair("ctime", db::Value(data.ctime)),
		pair("size", db::Value(data.size)),
		pair("type", db::Value(data.contentType)),
	}));
	return version.getInteger("__oid");
}

void AssetLibrary::eraseVersion(int64_t id) {
	_server->perform([this, id] (const Server &serv, const db::Transaction &t) {
		return _component->getVersions().remove(t, id);
	});
}

void AssetLibrary::setAssetDownload(int64_t id, bool value) {
	_server->perform([this, id, value] (const Server &serv, const db::Transaction &t) {
		return _component->getAssets().update(t, id, db::Value({
			pair("download", db::Value(value))
		}));
	});
}

void AssetLibrary::setVersionComplete(int64_t id, bool value) {
	_server->perform([this, id, value] (const Server &serv, const db::Transaction &t) {
		return _component->getVersions().update(t, id, db::Value({
			pair("complete", db::Value(value))
		}));
	});
}

void AssetLibrary::cleanup() {
	if (!_application->isNetworkOnline()) {
		// if we had no network connection to restore assets - do not cleanup them
		return;
	}

	_server->perform([this] (const storage::Server &serv, const db::Transaction &t) {
		_component->cleanup(t);
		return true;
	}, this);
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

void AssetLibrary::handleLibraryLoaded(Vector<Rc<Asset>> &&assets) {
	for (auto &it : assets) {
		StringView url = it->getUrl();

		_assetsByUrl.emplace(it->getUrl(), it.get());
		_assetsById.emplace(it->getId(), it.get());

		auto iit = _callbacks.find(url);
		if (iit != _callbacks.end()) {
			for (auto &cb : iit->second) {
				cb.first(it);
			}
		}
	}

	assets.clear();

	_loaded = true;

	for (auto &it : _tmpRequests) {
		acquireAsset(it.url, move(it.callback), it.ttl, move(it.ref));
	}

	_tmpRequests.clear();

	for (auto &it : _tmpMultiRequest) {
		acquireAssets(it.vec, move(it.callback), move(it.ref));
	}

	_tmpMultiRequest.clear();
}

void AssetLibrary::handleAssetLoaded(Rc<Asset> &&asset) {
	_application->performOnMainThread([this, asset = move(asset)] {
		_assetsById.emplace(asset->getId(), asset);
		_assetsByUrl.emplace(asset->getUrl(), asset);

		auto it = _callbacks.find(asset->getUrl());
		if (it != _callbacks.end()) {
			for (auto &cb : it->second) {
				if (cb.first) {
					cb.first(asset);
				}
			}
			_callbacks.erase(it);
		}
	}, this);
}

}
