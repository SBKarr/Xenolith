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

class AssetStorageServer : public Server {
public:
	virtual bool init(AssetLibrary *, const data::Value &params);

	AssetStorage *getStorage() const { return _storage; }

protected:
	AssetStorage *_storage = nullptr;
};

class AssetLibrary : public Ref {
public:
	static EventHeader onLoaded;

	using AssetCallback = Function<void (const Rc<Asset> &)>;
	using AssetVecCallback = Function<void (const SpanView<Rc<Asset>> &)>;

	struct AssetRequest {
		String url;
		AssetCallback callback;
		TimeInterval ttl;

		AssetRequest(StringView, AssetCallback &&, TimeInterval);
	};

	static String getTempPath(StringView);
	static String getAssetUrl(StringView);

	virtual ~AssetLibrary();

	bool init(Application *, const data::Value &dbParams);

	void onComponentLoaded();
	void onComponentDisposed();

	void setServerDate(const Time &t);

	// acquire single asset with url and ttl
	// asset can be:
	// - network asset, url started with some network scheme (`http://`. `https://`)
	// - application asset, url started with `app://`
	// - file asset, url started with `/` (absolute path) or '%` (app-relative path)
	// all others urls converted into app urls
	bool acquireAsset(StringView url, AssetCallback &&cb, TimeInterval ttl = TimeInterval());

	// acquire multiple assets with optional single competition callback
	bool acquireAssets(const SpanView<AssetRequest> &, AssetVecCallback && = nullptr);

	Application *getApplication() const { return _application; }

	Asset *getLiveAsset(StringView) const;
	Asset *getLiveAsset(int64_t) const;

	/*

	bool isLoaded() const { return _loaded; }

	bool isLiveAsset(uint64_t) const;
	bool isLiveAsset(const StringView &url, const StringView &path) const;

	void updateAssets();

	void addAssetFile(const StringView &, const StringView &, uint64_t asset, uint64_t ctime);
	void removeAssetFile(const StringView &);

	void finalize();*/

protected:
	friend class Asset;

	void removeAsset(Asset *);

	network::Handle * downloadAsset(Asset *);

	friend class AssetStorage;

	void cleanup();

	// void removeDownload(network::AssetHandle *);

	Time getCorrectTime() const;

	void notifyAssetCallbacks(Rc<Asset> &&);

	bool _loaded = false;
	int64_t _dt = 0;

	Vector<AssetRequest> _tmpRequests;
	Vector<Pair<Vector<AssetRequest>, AssetVecCallback>> _tmpMultiRequest;

	Map<String, Vector<AssetCallback>> _callbacks;

	Map<StringView, Asset *> _assetsByUrl;
	Map<uint64_t, Asset *> _assetsById;
	Map<Asset *, Rc<network::AssetHandle>> _downloads;

	Application *_application = nullptr;
	Rc<AssetStorageServer> _server;
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLASSETLIBRARY_H_ */
