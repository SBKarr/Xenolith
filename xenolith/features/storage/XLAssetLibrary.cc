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

XL_DECLARE_EVENT_CLASS(AssetLibrary, onLoaded)

String AssetLibrary::getTempPath(StringView path) {
	return toString(path, ".tmp");
}

bool AssetLibrary::init(Application *, storage::Server::Builder &builder, StringView name) {
	_storage = builder.addComponent(new (builder.getPool()) AssetStorage(this, name));
	return true;
}

void AssetLibrary::onComponentLoaded() {
	for (auto &it : _downloads) {
		it.second->perform(_application, nullptr);
	}

	_loaded = true;
	getAssets(_tmpRequests);
	for (auto &it : _tmpMultiRequest) {
		getAssets(it.first, it.second);
	}
	onLoaded(this);
}

void AssetLibrary::onComponentDisposed() {
	_storage = nullptr;
}

void AssetLibrary::cleanup() {
	if (!_application->isNetworkOnline()) {
		// if we had no network connection to restore assets - do not cleanup them
		return;
	}

	if (!_storage) {
		return;
	}

	_storage->getServer()->perform([storage = _storage] (const storage::Server &serv, const db::Transaction &t) {
		storage->cleanup(t);
		return true;
	});
}

void AssetLibrary::removeDownload(network::AssetHandle *d) {
	auto a = d->getAsset();
	_downloads.erase(a);
}

bool AssetLibrary::isLiveAsset(uint64_t id) const {
	return getLiveAsset(id) != nullptr;
}
bool AssetLibrary::isLiveAsset(const StringView &url, const StringView &path) const {
	return getLiveAsset(url, path) != nullptr;
}

void AssetLibrary::updateAssets() {
	for (auto &it : _assets) {
		it.second->checkFile();
	}
}

void AssetLibrary::addAssetFile(const StringView &path, const StringView &url, uint64_t asset, uint64_t ctime) {
	_stateClass.insert(data::Value({
		pair("path", data::Value(path)),
		pair("url", data::Value(url)),
		pair("ctime", data::Value(int64_t(ctime))),
		pair("asset", data::Value(int64_t(asset))),
	}))->perform();
}
void AssetLibrary::removeAssetFile(const StringView &path) {
	_stateClass.remove()->select(path)->perform();
}

Time AssetLibrary::getCorrectTime() const {
	return Time::microseconds(Time::now().toMicros() + _dt);
}

Asset *AssetLibrary::getLiveAsset(uint64_t id) const {
	auto it = _assets.find(id);
	if (it != _assets.end()) {
		return it->second;
	}
	return nullptr;
}

Asset *AssetLibrary::getLiveAsset(const StringView &url, const StringView &path) const {
	return getLiveAsset(getAssetId(url, path));
}

uint64_t AssetLibrary::getAssetId(const StringView &url, const StringView &path) const {
	return string::stdlibHashUnsigned(toString(url, "|", filepath::canonical(path)));
}


void AssetLibrary::setServerDate(const Time &serv) {
	auto t = Time::now();
	if (t < serv) {
		_correctionNegative = false;
	} else {
		_correctionNegative = true;
	}
	_correctionTime = serv - t;

	data::Value d;
	d.setInteger(_correctionTime.toMicroseconds(), "time");
	d.setInteger(_correctionNegative, "negative");

	storage::set("SP.AssetLibrary.Time", move(d), nullptr, getAssetStorage());
}

bool AssetLibrary::getAsset(const AssetCallback &cb, const StringView &url,
		const StringView &path, TimeInterval ttl, const StringView &cache, const DownloadCallback &dcb) {
	if (!_loaded) {
		_tmpRequests.push_back(AssetRequest(cb, url, path, ttl, cache, dcb));
		return true;
	}

	uint64_t id = getAssetId(url, path);
	if (auto a = getLiveAsset(id)) {
		if (cb) {
			cb(a);
			return true;
		}
	}
	auto it = _callbacks.find(id);
	if (it != _callbacks.end()) {
		it->second.push_back(cb);
	} else {
		CallbackVec vec;
		vec.push_back(cb);
		_callbacks.insert(pair(id, move(vec)));

		auto &thread = storage::thread(getAssetStorage());
		Asset ** assetPtr = new (Asset *) (nullptr);
		thread.perform([this, assetPtr, id, url = url.str(), path = path.str(), cache = cache.str(), ttl, dcb] (const thread::Task &) -> bool {
			data::Value data;
			_assetsClass.get([&data] (data::Value &&d) {
				if (d.isArray() && d.size() > 0) {
					data = move(d.getValue(0));
				}
			})->select(id)->perform();

			if (!data) {
				data.setInteger(reinterpretValue<int64_t>(id), "id");
				data.setString(url, "url");
				data.setString(filepath::canonical(path), "path");
				data.setInteger(ttl.toMicroseconds(), "ttl");
				data.setString(filepath::canonical(cache), "cache");
				data.setInteger(getCorrectTime().toMicroseconds(), "touch");
				_assetsClass.insert(data)->perform();
			}

			if (data.isDictionary()) {
				data.setString(filepath::absolute(data.getString("path")), "path");
				data.setString(filepath::absolute(data.getString("cache")), "cache");
			}

			(*assetPtr) = new Asset(data, dcb);
			return true;
		}, [this, assetPtr] (const thread::Task &, bool) {
			onAssetCreated(*assetPtr);
			delete assetPtr;
		}, this);
	}

	return true;
}

AssetLibrary::AssetRequest::AssetRequest(const AssetCallback &cb, const StringView &url, const StringView &path,
		TimeInterval ttl, const StringView &cacheDir, const DownloadCallback &dcb)
: callback(cb), id(AssetLibrary::getInstance()->getAssetId(url, path))
, url(url.str()), path(path.str()), ttl(ttl), cacheDir(cacheDir.str()), download(dcb) { }

bool AssetLibrary::getAssets(const Vector<AssetRequest> &vec, const AssetVecCallback &cb) {
	if (!_loaded) {
		if (!cb) {
			for (auto &it : vec) {
				_tmpRequests.push_back(it);
			}
		} else {
			_tmpMultiRequest.push_back(pair(vec, cb));
		}
		return true;
	}

	size_t assetCount = vec.size();
	auto requests = new Vector<AssetRequest>;
	AssetVec *retVec = nullptr;
	if (cb) {
		retVec = new AssetVec;
	}
	for (auto &it : vec) {
		uint64_t id = it.id;
		if (auto a = getLiveAsset(id)) {
			if (it.callback) {
				it.callback(a);
			}
			if (retVec) {
				retVec->emplace_back(a);
			}
		} else {
			auto cbit = _callbacks.find(id);
			if (cbit != _callbacks.end()) {
				cbit->second.push_back(it.callback);
				if (cb && retVec) {
					cbit->second.push_back([cb, retVec, assetCount] (Asset *a) {
						retVec->emplace_back(a);
						if (retVec->size() == assetCount) {
							cb(*retVec);
							delete retVec;
						}
					});
				}
			} else {
				CallbackVec vec;
				vec.push_back(it.callback);
				_callbacks.insert(pair(id, move(vec)));
				requests->push_back(it);
			}
		}
	}

	if (requests->empty()) {
		if (cb && retVec) {
			if (retVec->size() == assetCount) {
				cb(*retVec);
				delete retVec;
			}
		}
		delete requests;
		return true;
	}

	auto &thread = storage::thread(getAssetStorage());
	auto assetsVec = new AssetVec;
	thread.perform([this, assetsVec, requests] (const thread::Task &) -> bool {
		performGetAssets(*assetsVec, *requests);

		return true;
	}, [this, assetsVec, requests, retVec, assetCount, cb] (const thread::Task &, bool) {
		for (auto &it : (*assetsVec)) {
			if (retVec) {
				retVec->emplace_back(it);
			}
			onAssetCreated(it);
		}
		if (cb && retVec) {
			if (retVec->size() == assetCount) {
				cb(*retVec);
				delete retVec;
			}
		}
		delete requests;
		delete assetsVec;
	}, this);

	return true;
}

void AssetLibrary::performGetAssets(AssetVec &assetsVec, const Vector<AssetRequest> &requests) {
	data::Value data;
	auto cmd = _assetsClass.get([&data] (data::Value &&d) {
		if (d.isArray() && d.size() > 0) {
			data = move(d);
		}
	});
	for (auto &it : requests) {
		cmd->select(it.id);
	}
	cmd->perform();

	Map<uint64_t, data::Value> dataMap;
	if (data) {
		for (auto &it : data.asArray()) {
			auto id = it.getInteger("id");
			dataMap.insert(pair(id, move(it)));
		}
	}

	data = data::Value(data::Value::Type::ARRAY);

	for (auto &it : requests) {
		auto dataIt = dataMap.find(it.id);
		data::Value rdata;
		if (dataIt == dataMap.end()) {
			rdata.setInteger(reinterpretValue<int64_t>(it.id), "id");
			rdata.setString(it.url, "url");
			rdata.setString(filepath::canonical(it.path), "path");
			rdata.setInteger(it.ttl.toMicroseconds(), "ttl");
			rdata.setString(filepath::canonical(it.cacheDir), "cache");
			rdata.setInteger(getCorrectTime().toMicroseconds(), "touch");
			data.addValue(rdata);
		} else {
			rdata = move(dataIt->second);
		}

		if (rdata.isDictionary()) {
			rdata.setString(filepath::absolute(rdata.getString("path")), "path");
			rdata.setString(filepath::absolute(rdata.getString("cache")), "cache");
		}

		auto a = new Asset(rdata, it.download);
		assetsVec.push_back(a);
	}

	if (data.size() > 0) {
		 _assetsClass.insert(move(data))->perform();
	}
}

Asset *AssetLibrary::acquireLiveAsset(const StringView &url, const StringView &path) {
	uint64_t id = getAssetId(url, path);
	return getLiveAsset(id);
}

void AssetLibrary::onAssetCreated(Asset *asset) {
	if (asset) {
		_assets.insert(pair(asset->getId(), asset));
		auto it = _callbacks.find(asset->getId());
		if (it != _callbacks.end()) {
			for (auto &cb : it->second) {
				if (cb) {
					cb(asset);
				}
			}
			_callbacks.erase(it);
		}
	}
}

void AssetLibrary::removeAsset(Asset *asset) {
	_assets.erase(asset->getId());
	if (asset->isStorageDirty()) {
		saveAsset(asset);
	}
}

AssetDownload * AssetLibrary::downloadAsset(Asset *asset) {
	auto it = _downloads.find(asset);
	if (it == _downloads.end()) {
		if (Device::getInstance()->isNetworkOnline()) {
			auto d = Rc<AssetDownload>::create(asset, getTempPath(asset->getFilePath()));
			_downloads.emplace(asset, d);
			startDownload(d.get());
			return d;
		}
		return nullptr;
	} else {
		return it->second;
	}
}

void AssetLibrary::touchAsset(Asset *asset) {
	asset->touchWithTime(getCorrectTime());
	saveAsset(asset);
}

void AssetLibrary::saveAsset(Asset *asset) {
	data::Value val;
	val.setInteger(reinterpretValue<int64_t>(asset->getId()), "id");
	val.setInteger(asset->getSize(), "size");
	val.setInteger(asset->getMTime(), "mtime");
	val.setInteger(asset->getTouch().toMicroseconds(), "touch");
	val.setInteger(asset->getTtl().toMicroseconds(), "ttl");
	val.setBool(asset->isUpdateAvailable() || asset->isDownloadInProgress(), "unupdated");
	val.setInteger((_downloads.find(asset) != _downloads.end()), "download");
	val.setString(asset->getUrl(), "url");
	val.setString(filepath::canonical(asset->getFilePath()), "path");
	val.setString(filepath::canonical(asset->getCachePath()), "cache");
	val.setString(asset->getContentType(), "contentType");
	val.setString(asset->getETag(), "etag");
	val.setString(data::toString(asset->getData()), "data");

	_assetsClass.insert(move(val))->perform();

	asset->setStorageDirty(false);
}

AssetStorage::AssetStorage(AssetLibrary *lib, StringView name) : ServerComponent(name), _library(lib) {
	using namespace db;

	define(_assetsScheme, mem::Vector<Field>({
		Field::Integer("size"),
		Field::Integer("mtime", Flags::AutoMTime),
		Field::Integer("touch"),
		Field::Integer("ttl"),
		Field::Boolean("unupdated"),
		Field::Boolean("download"),
		Field::Text("url", MaxLength(2_KiB), Transform::Url),
		Field::Text("path", MaxLength(2_KiB)),
		Field::Text("cache", MaxLength(2_KiB)),
		Field::Text("contentType", MaxLength(2_KiB)),
		Field::Text("etag", MaxLength(2_KiB)),
		Field::Set("states", _stateScheme)
	}));

	define(_stateScheme, mem::Vector<Field>({
		Field::Text("path", Flags::Indexed),
		Field::Text("url", MaxLength(2_KiB), Transform::Url),
		Field::Integer("ctime", Flags::AutoCTime),
		Field::Object("asset", _assetsScheme, RemovePolicy::Cascade),
	}));
}

void AssetStorage::onStorageInit(storage::Server &serv, const db::Adapter &a) {
	auto v = a.get("XL.AssetLibrary.dt");

	_library->_dt = v.getInteger("dt");

	if (auto t = db::Transaction::acquire(a)) {
		_assetsScheme.foreach(t, db::Query().select("download", data::Value(true)), [&] (db::mem::Value &val) -> bool {
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
		}

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
		iface->performSimpleSelect(toString("SELECT path, cache FROM ", _assetsScheme.getName(),
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

		iface->performSimpleSelect(toString("SELECT path FROM ", _stateScheme.getName(), ";"), [&] (db::Result &res) {
			for (auto it : res) {
				auto path = filepath::absolute(it.toString(0));
				filesystem::remove(path);
			}
		});

		iface->performSimpleQuery(toString("DELETE FROM ", _stateScheme.getName()));
	}
}

}
