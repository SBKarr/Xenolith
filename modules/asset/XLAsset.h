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
#include "SPSyncRWLock.h"

namespace stappler::xenolith::storage {

class Asset;
class AssetLibrary;

/*class AssetFile : public Ref {
public:
	~AssetFile();

	bool init(Asset *);

	Bytes readFile() const;
	filesystem::ifile open() const;

	void remove();

	bool match(const Asset *) const;
	bool exists() const { return _exists; }
	operator bool() const { return _exists; }

	uint64_t getCTime() const { return _ctime; }
	uint64_t getMTime() const { return _mtime; }
	uint64_t getAssetId() const { return _id; }
	size_t getSize() const { return _size; }

	StringView getPath() const { return _path; }
	StringView getUrl() const { return _url; }
	StringView getContentType() const { return _contentType; }
	StringView getETag() const { return _etag; }

protected:
	friend class Asset;

	bool _exists = false;
	uint64_t _ctime = 0;
	uint64_t _mtime = 0;
	uint64_t _id = 0;
	size_t _size = 0;

	String _url;
	String _path;
	String _contentType;
	String _etag;
};*/

class Asset : public data::Subscription {
public:
	enum Update : uint8_t {
		CacheDataUpdated = 2,
		FileUpdated,
		DownloadStarted,
		DownloadProgress,
		DownloadCompleted,
		DownloadSuccessful,
		DownloadFailed,
		WriteLocked,
		ReadLocked,
		Unlocked,
	};

	struct Data {
		bool success = false;
		Time ctime; // creation time
		Time mtime; // last modification time
		Time atime; // last access time (if supported)
		size_t size = 0; // file size
		uint64_t version = 0; // version id (if available)

		bool download = false; // is download active for file
		float progress = 0.0f; // download progress

		bool isFile = true; // is direct file access available (so, path can be opened with filesystem::open)
		String path;
	};

	Asset(AssetLibrary *, const db::mem::Value &);
	virtual ~Asset();

	// try to read asset data, returns false if no data available
	// also can fail with callback (.success = false, BytesView()) with no data
	bool read(Function<void(Data, BytesView)> &&);

	// get info about asset
	bool info(Function<void(Data)> &&);

	int64_t getId() const { return _id; }
	StringView getUrl() const { return _url; }

	/*bool download();
	void clear();
	void checkFile();

	bool isReadAvailable() const;
	bool isDownloadAvailable() const;
	bool isUpdateAvailable() const;

	StringView getFilePath() const { return _path; }
	StringView getCachePath() const { return _cachePath; }
	StringView getContentType() const { return _contentType; }

	bool isDownloadInProgress() const { return _downloadInProgress; }
	float getProgress() const { return _progress; }

	uint64_t getMTime() const { return _mtime; }
	size_t getSize() const { return _size; }
	StringView getETag() const { return _etag; }

	Time getTouch() const { return _touch; }
	TimeInterval getTtl() const { return _ttl; }

	void setData(const data::Value &d) { _data = d; _storageDirty = true; }
	void setData(data::Value &&d) { _data = std::move(d); _storageDirty = true; }
	const data::Value &getData() const { return _data; }

	bool isFileExists() const { return _fileExisted; }
	bool isFileUpdate() const { return _fileUpdate; }

	void onStarted();
	void onProgress(float progress);
	void onCompleted(bool success, bool cacheRequest, const StringView &file, const StringView &ct, const StringView &etag, uint64_t mtime, size_t size);
	void onFile();

	void save();
	void touch();

	bool isStorageDirty() const { return _storageDirty; }
	void setStorageDirty(bool value) { _storageDirty = value; }

	// Rc<AssetFile> cloneFile();*/

protected:
	friend class AssetLibrary;

	/*void update(Update);
	bool swapFiles(const StringView &file, const StringView &ct, const StringView &etag, uint64_t mtime, size_t size);
	void touchWithTime(Time t);

	virtual void onLocked(Lock) override;


	String _path;
	String _cachePath;
	String _contentType;

	Time _touch;
	TimeInterval _ttl;

	uint64_t _mtime = 0;
	size_t _size = 0;
	String _etag;

	float _progress = 0;

	bool _storageDirty = false;

	bool _fileExisted = false;
	bool _unupdated = false;
	bool _waitFileSwap = false;
	bool _downloadInProgress = false;
	bool _fileUpdate = false;*/

	int64_t _id = 0;
	String _url;
	data::Value _data;
	AssetLibrary *_library = nullptr;
};

}

#endif /* XENOLITH_FEATURES_STORAGE_XLASSET_H_ */
