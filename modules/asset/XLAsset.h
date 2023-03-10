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

class Asset : public Subscription {
public:
	enum Update : uint8_t {
		CacheDataUpdated = 2,
		FileUpdated,
		DownloadStarted,
		DownloadProgress,
		DownloadCompleted,
		DownloadSuccessful,
		DownloadFailed,
	};

	struct VersionData {
		bool complete = false;
		bool download = false; // is download active for file
		bool isFile = true; // is direct file access available (so, path can be opened with filesystem::open)
		bool isLocal = false;
		int64_t id = 0;
		Time ctime; // creation time
		Time mtime; // last modification time
		size_t size = 0; // file size
		float progress = 0.0f; // download progress

		String path;
		String contentType;
		String etag;
	};

	Asset(AssetLibrary *, const db::Value &);
	virtual ~Asset();

	// try to read asset data, returns false if no data available
	// also can fail with callback (.success = false, BytesView()) with no data
	bool read(Callback<void(VersionData, BytesView)> &&);

	// get info about asset
	bool info(Callback<void(VersionData)> &&);

	int64_t getId() const { return _id; }
	StringView getUrl() const { return _url; }
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

	/*
	void checkFile();

	bool isReadAvailable() const;
	bool isDownloadAvailable() const;
	bool isUpdateAvailable() const;

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
