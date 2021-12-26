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

#ifndef XENOLITH_FEATURES_STORAGE_XLASSETLIBRARY_H_
#define XENOLITH_FEATURES_STORAGE_XLASSETLIBRARY_H_

#include "XLAsset.h"
#include "XLEventHeader.h"
#include "XLStorageServer.h"

namespace stappler::xenolith::storage {

class AssetStorage;

class AssetLibrary : public Ref {
public:
	static EventHeader onLoaded;

	struct AssetRequest;

	using AssetCallback = Function<void (Asset *)>;

	using AssetVec = Vector<Rc<Asset>>;
	using AssetVecCallback = Function<void (const AssetVec &)>;

	using AssetRequestVec = Vector<AssetRequest>;
	using AssetMultiRequestVec = Vector<Pair<AssetRequestVec, AssetVecCallback>>;

	using DownloadCallback = Asset::DownloadCallback;

	using CallbackVec = Vector<AssetCallback>;
	using CallbacksList = Map<uint64_t, CallbackVec>;

	struct AssetRequest {
		AssetCallback callback;
		uint64_t id;
		String url;
		String path;
		TimeInterval ttl;
		String cacheDir;
		DownloadCallback download;

		AssetRequest(const AssetCallback &, const StringView &url, const StringView &path,
				TimeInterval ttl = TimeInterval(), const StringView &cacheDir = StringView(), const DownloadCallback & = nullptr);
	};

	static String getTempPath(StringView);

	bool init(Application *, storage::Server::Builder &, StringView name);

	void onComponentLoaded();
	void onComponentDisposed();

	void setServerDate(const Time &t);

	/* get asset from db, new asset is autoreleased */
	bool getAsset(const AssetCallback &cb, const StringView &url, const StringView &path,
			TimeInterval ttl = TimeInterval(), const StringView &cacheDir = StringView(), const DownloadCallback & = nullptr);

	bool getAssets(const AssetRequestVec &, const AssetVecCallback & = nullptr);

	Asset *acquireLiveAsset(const StringView &url, const StringView &path);

	bool isLoaded() const { return _loaded; }
	uint64_t getAssetId(const StringView &url, const StringView &path) const;

	bool isLiveAsset(uint64_t) const;
	bool isLiveAsset(const StringView &url, const StringView &path) const;

	void updateAssets();

	void addAssetFile(const StringView &, const StringView &, uint64_t asset, uint64_t ctime);
	void removeAssetFile(const StringView &);

	void finalize();

protected:
	friend class Asset;

	void removeAsset(Asset *);
	void saveAsset(Asset *);
	void touchAsset(Asset *);

	network::Handle * downloadAsset(Asset *);

protected:
	friend class AssetStorage;

	void cleanup();

	void removeDownload(network::AssetHandle *);

	Time getCorrectTime() const;
	Asset *getLiveAsset(uint64_t id) const;
	Asset *getLiveAsset(const StringView &, const StringView &) const;

	void onAssetCreated(Asset *);

	void performGetAssets(AssetVec &, const Vector<AssetRequest> &);

	Application *_application = nullptr;
	bool _loaded = false;

	Map<uint64_t, Asset *> _assets;
	Map<Asset *, Rc<network::AssetHandle>> _downloads;

	std::mutex _mutex;

	int64_t _dt = 0;

	AssetRequestVec _tmpRequests;
	AssetMultiRequestVec _tmpMultiRequest;

	CallbacksList _callbacks;
	AssetStorage *_storage = nullptr;
};

class AssetStorage : public ServerComponent {
public:
	AssetStorage(AssetLibrary *, StringView name);

	virtual void onStorageInit(storage::Server &, const db::Adapter &) override;
	virtual void onComponentLoaded() override;
	virtual void onComponentDisposed() override;

	void cleanup(const db::Transaction &t);

protected:
	AssetLibrary *_library = nullptr;
	Scheme _assetsScheme = Scheme("assets");
	Scheme _stateScheme = Scheme("state");
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLASSETLIBRARY_H_ */
