/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_FEATURES_STORAGE_XLASSET_H_
#define XENOLITH_FEATURES_STORAGE_XLASSET_H_

#include "XLNetworkRequest.h"
#include "XLDefine.h"
#include "XLStorageServer.h"
#include "SPSubscription.h"

namespace stappler::xenolith::storage {

class AssetLibrary;
struct AssetDownloadData;

struct AssetVersionData {
	bool complete = false;
	bool download = false; // is download active for file
	uint32_t locked = 0;
	int64_t id = 0;
	Time ctime; // creation time
	Time mtime; // last modification time
	size_t size = 0; // file size
	float progress = 0.0f; // download progress

	String path;
	String contentType;
	String etag;
};

class Asset;

class AssetLock : public Ref {
public:
	using VersionData = AssetVersionData;

	virtual ~AssetLock();

	int64_t getId() const { return _lockedVersion.id; }
	Time getCTime() const { return _lockedVersion.ctime; }
	Time getMTime() const { return _lockedVersion.mtime; }
	size_t getSize() const { return _lockedVersion.size; }

	StringView getPath() const { return _lockedVersion.path; }
	StringView getContentType() const { return _lockedVersion.contentType; }
	StringView getEtag() const { return _lockedVersion.etag; }
	StringView getCachePath() const;

	const Rc<Asset> &getAsset() const { return _asset; }

protected:
	friend class Asset;

	AssetLock(Rc<Asset> &&, const VersionData &, Function<void(const VersionData &)> &&);

	VersionData _lockedVersion;
	Function<void(const VersionData &)> _releaseFunction;
	Rc<Asset> _asset;
};

class Asset : public Subscription {
public:
	using VersionData = AssetVersionData;

	enum Update : uint8_t {
		CacheDataUpdated = 1 << 1,
		DownloadStarted = 1 << 2,
		DownloadProgress = 1 << 3,
		DownloadCompleted = 1 << 4,
		DownloadSuccessful = 1 << 5,
		DownloadFailed = 1 << 6,
	};

	Asset(AssetLibrary *, const db::Value &);
	virtual ~Asset();

	const VersionData *getReadableVersion() const;

	Rc<AssetLock> lockVersion(int64_t);

	int64_t getId() const { return _id; }
	StringView getUrl() const { return _url; }
	StringView getCachePath() const { return _cache; }
	Time getTouch() const { return _touch; }
	TimeInterval getTtl() const { return _ttl; }

	bool download();
	void touch(Time t = Time::now());
	void clear();

	bool isDownloadInProgress() const;
	float getProgress() const;

	bool isStorageDirty() const { return _dirty; }
	void setStorageDirty(bool value) { _dirty = value; }

	void setData(const Value &d);
	void setData(Value &&d);
	const Value &getData() const { return _data; }

	Value encode() const;

	/*bool isUpdateAvailable() const;

	StringView getFilePath() const { return _path; }
	StringView getCachePath() const { return _cachePath; }
	StringView getContentType() const { return _contentType; }


	bool isFileExists() const { return _fileExisted; }
	bool isFileUpdate() const { return _fileUpdate; }

	void onStarted();
	void onProgress(float progress);
	void onCompleted(bool success, bool cacheRequest, const StringView &file, const StringView &ct, const StringView &etag, uint64_t mtime, size_t size);
	void onFile();

	void save();
	void touch();

	 */

protected:
	friend class AssetLibrary;

	void update(Update);

	void parseVersions(const db::Value &);
	bool startNewDownload(Time ctime, StringView etag);
	bool resumeDownload(VersionData &);

	void setDownloadProgress(int64_t, float progress);
	void setDownloadComplete(VersionData &, bool success);
	void setFileValidated(bool success);
	void replaceVersion(VersionData &);

	// called from network thread
	void addVersion(AssetDownloadData *);
	void dropVersion(const VersionData &);

	void releaseLock(const VersionData &);

	String _path;
	String _cache;
	String _url;
	TimeInterval _ttl;
	Time _touch;
	Time _mtime;
	int64_t _id = 0;
	int64_t _downloadId = 0;

	Vector<VersionData> _versions;

	Value _data;
	AssetLibrary *_library = nullptr;
	bool _download = false;
	bool _dirty = true;
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLASSET_H_ */
