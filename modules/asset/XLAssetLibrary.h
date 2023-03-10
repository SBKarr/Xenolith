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
#include "XLStorageComponent.h"

namespace stappler::xenolith::storage {

class AssetComponent;

class AssetLibrary : public ComponentContainer {
public:
	static EventHeader onLoaded;

	using AssetCallback = Function<void (const Rc<Asset> &)>;
	using AssetVecCallback = Function<void (const SpanView<Rc<Asset>> &)>;

	struct AssetRequest {
		String url;
		AssetCallback callback;
		TimeInterval ttl;
		Rc<Ref> ref;

		AssetRequest(StringView url, AssetCallback &&cb, TimeInterval ttl, Rc<Ref> &&ref)
		: url(AssetLibrary::getAssetUrl(url)), callback(move(cb)), ttl(ttl), ref(move(ref)) { }
	};

	struct AssetMultiRequest {
		Vector<AssetRequest> vec;
		AssetVecCallback callback;
		Rc<Ref> ref;

		AssetMultiRequest(Vector<AssetRequest> &&vec, AssetVecCallback &&cb, Rc<Ref> &&ref)
		: vec(move(vec)), callback(move(cb)), ref(move(ref)) { }
	};

	static String getAssetPath(int64_t);
	static String getAssetUrl(StringView);

	virtual ~AssetLibrary();

	bool init(Application *, const Value &dbParams);

	void update(uint64_t clock);

	virtual void handleStorageInit(storage::ComponentLoader &loader) override;
	virtual void handleStorageDisposed(const db::Transaction &t) override;

	bool acquireAsset(StringView url, AssetCallback &&cb, TimeInterval ttl = TimeInterval(), Rc<Ref> && = nullptr);
	bool acquireAssets(SpanView<AssetRequest>, AssetVecCallback && = nullptr, Rc<Ref> && = nullptr);

	Asset *getLiveAsset(StringView) const;
	Asset *getLiveAsset(int64_t) const;

	Application *getApplication() const { return _application; }

protected:
	friend class Asset;
	friend class AssetComponent;

	int64_t addVersion(const db::Transaction &t, int64_t assetId, const Asset::VersionData &);
	void eraseVersion(int64_t);

	void setAssetDownload(int64_t id, bool value);
	void setVersionComplete(int64_t id, bool value);

	void removeAsset(Asset *);

	network::Handle * downloadAsset(Asset *);

	void cleanup();

	void handleLibraryLoaded(Vector<Rc<Asset>> &&assets);
	void handleAssetLoaded(Rc<Asset> &&);

	bool _loaded = false;
	Map<String, Vector<Pair<AssetCallback, Rc<Ref>>>> _callbacks;

	Vector<Rc<Asset>> _liveAssets;
	Map<StringView, Asset *> _assetsByUrl;
	Map<uint64_t, Asset *> _assetsById;

	Application *_application = nullptr;
	AssetComponent *_component = nullptr;
	Rc<Server> _server;

	Vector<AssetRequest> _tmpRequests;
	Vector<AssetMultiRequest> _tmpMultiRequest;
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLASSETLIBRARY_H_ */
