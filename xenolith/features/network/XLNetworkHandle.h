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

#ifndef XENOLITH_FEATURES_NETWORK_XLNETWORKHANDLE_H_
#define XENOLITH_FEATURES_NETWORK_XLNETWORKHANDLE_H_

#include "XLDefine.h"
#include "SPNetworkHandle.h"

namespace stappler::xenolith::network {

class Controller;

using Method = NetworkHandle::Method;

class Handle : public NetworkHandle, public Ref {
public:
	using CompleteCallback = Function<void(Handle &)>;
	using ProgressCallback = Function<void(Handle &, int64_t total, int64_t now)>;

	virtual ~Handle() { }

	// just GET url, actions with data defined with setSend*/setReceive*
	virtual bool init(StringView url);

	// download to file with GET
	virtual bool init(StringView url, FilePath fileName);

	// perform query with specific method, actions with data defined with setSend*/setReceive*
	virtual bool init(Method method, StringView url);

	bool isSuccess() const { return _success; }
	int64_t getMTime() const { return _mtime; }
	StringView getETag() const { return _etag; }
	StringView getSharegroup() const { return _sharegroup; }

	void setMTime(int64_t val) { _mtime = val; }
	void setETag(StringView val) { _etag = val.str(); }
	void setSharegroup(StringView val) { _sharegroup = val.str(); }

	float getUploadProgress() const { return (float)(_uploadProgress.second) / (float)(_uploadProgress.first); }
	float getDownloadProgress() const { return (float)(_downloadProgress.second) / (float)(_downloadProgress.first); }

	Pair<int64_t, int64_t> getUploadProgressCounters() const { return _uploadProgress; }
	Pair<int64_t, int64_t> getDownloadProgressCounters() const { return _downloadProgress; }

	virtual void setUploadProgress(ProgressCallback &&);
	virtual void setDownloadProgress(ProgressCallback &&);

	void setSignRequest(bool value) { _signRequest = value; }
	bool shouldSignRequest() const { return _signRequest; }

	virtual void perform(Application *, CompleteCallback &&cb);

protected:
	friend class Controller;

	virtual void notifyOnComplete();
    virtual void notifyOnUploadProgress(int64_t total, int64_t now);
    virtual void notifyOnDownloadProgress(int64_t total, int64_t now);

	virtual bool prepare(Context *, const Callback<bool(CURL *)> &onBeforePerform) override;
	virtual bool finalize(Context *, const Callback<bool(CURL *)> &onAfterPerform) override;

	bool _success = false;
	bool _signRequest = false;
	std::array<char, 256> _errorBuffer = { 0 };

	uint64_t _mtime = 0;
	String _etag;
	String _sharegroup;

	ProgressCallback _onDownloadProgress;
	ProgressCallback _onUploadProgress;
	CompleteCallback _onComplete;
	Pair<int64_t, int64_t> _uploadProgress; // total, now
	Pair<int64_t, int64_t> _downloadProgress; // total, now
};

class DataHandle : public Handle {
public:
	using DataCompleteCallback = Function<void(Handle &, data::Value &)>;

	virtual ~DataHandle() { }

	virtual bool init(StringView url, const data::Value &data = data::Value(), data::EncodeFormat = data::EncodeFormat::Cbor);
	virtual bool init(Method method, StringView url, const data::Value &data = data::Value(), data::EncodeFormat = data::EncodeFormat::Cbor);
	virtual bool init(Method method, StringView url, FilePath file, StringView type = StringView());

	virtual void perform(Application *, DataCompleteCallback &&cb);

protected:
	virtual bool prepare(Context *, const Callback<bool(CURL *)> &onBeforePerform) override;

	Bytes _data;
	size_t _offset = 0;
};

class AssetHandle : public Handle {
public:
	virtual ~AssetHandle();

	virtual bool init(Rc<storage::Asset> &&, StringView tmp);

	Rc<storage::Asset> getAsset() const;

protected:
	Rc<storage::Asset> _asset;
};

}

#endif /* XENOLITH_FEATURES_NETWORK_XLNETWORKHANDLE_H_ */
