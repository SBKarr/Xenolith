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

#include "XLNetworkHandle.h"
#include "XLNetworkController.h"
#include "XLApplication.h"

#include <curl/curl.h>

namespace stappler::xenolith::network {

bool Handle::init(StringView url) {
	return NetworkHandle::init(Method::Get, url);
}

bool Handle::init(StringView url, FilePath fileName) {
    if (!init(Method::Get, url)) {
        return false;
    }

    if (!fileName.get().empty()) {
    	setReceiveFile(fileName.get(), false);
    }
    return true;
}

bool Handle::init(Method method, StringView url) {
	return NetworkHandle::init(method, url);
}

void Handle::setUploadProgress(ProgressCallback &&cb) {
	_onUploadProgress = move(cb);
}

void Handle::setDownloadProgress(ProgressCallback &&cb) {
	_onDownloadProgress = move(cb);
}

void Handle::perform(Application *app, CompleteCallback &&cb) {
	if (cb) {
		_onComplete = move(cb);
	}
	if (!app) {
		app = Application::getInstance();
	}
	auto c = app->getNetworkController();
	c->run(this);
}

void Handle::notifyOnComplete() {
	if (_onComplete) {
		_onComplete(*this);
	}
}

void Handle::notifyOnUploadProgress(int64_t total, int64_t now) {
	_uploadProgress = pair(total, std::max(now, _uploadProgress.second)); // prevent out-of-order updates
	if (_onUploadProgress && _uploadProgress.second == now) {
		_onUploadProgress(*this, total, now);
	}
}

void Handle::notifyOnDownloadProgress(int64_t total, int64_t now) {
	_downloadProgress = pair(total, std::max(now, _downloadProgress.second)); // prevent out-of-order updates
	if (_onDownloadProgress && _downloadProgress.second == now) {
		_onDownloadProgress(*this, total, now);
	}
}

bool Handle::prepare(Context *ctx, const Callback<bool(CURL *)> &onBeforePerform) {
	auto controller = (Controller *)ctx->userdata;
	if (_mtime > 0) {
		ctx->headers = curl_slist_append(ctx->headers, toString("If-Modified-Since: ", Time::microseconds(_mtime).toHttp()).data());
	}
	if (!_etag.empty()) {
		ctx->headers = curl_slist_append(ctx->headers, toString("If-None-Match: ", _etag).data());
	}

	if (!_sharegroup.empty() && ctx->share) {
		_cookieFile = filesystem::writablePath(toString("network.", controller->getName(), ".", _sharegroup, ".cookies"));
	}

	setUserAgent(controller->getApplication()->getUserAgent());

	_uploadProgress = pair(0, 0);
	_downloadProgress = pair(0, 0);

	return NetworkHandle::prepare(ctx, onBeforePerform);
}

bool Handle::finalize(Context *ctx, const Callback<bool(CURL *)> &onAfterPerform) {
	auto ret = NetworkHandle::finalize(ctx, onAfterPerform);

	_success = ctx->success;

	if (_responseCode < 300) {
		_mtime = Time::fromHttp(getReceivedHeaderString("Last-Modified")).toMicroseconds();
		_etag = getReceivedHeaderString("ETag").str();
	} else {
		auto f = getReceiveFile();
		if (!f.empty()) {
			filesystem::remove(f);
		}
	}

	return ret;
}


bool DataHandle::init(StringView url, const data::Value &data, data::EncodeFormat format) {
	if (data.empty()) {
		return init(Method::Get, url);
	} else {
		return init(Method::Post, url, data, format);
	}
}

bool DataHandle::init(Method method, StringView url, const data::Value &data, data::EncodeFormat format) {
	if (!Handle::init(method, url)) {
		return false;
	}

	if ((method == Method::Post || method == Method::Put) && !data.isNull()) {
		setSendData(data, format);
	}

	return true;
}

bool DataHandle::init(Method method, StringView url, FilePath file, StringView type) {
	if (!Handle::init(method, url)) {
		return false;
	}

	if ((method == Method::Post || method == Method::Put) && !file.get().empty()) {
		addHeader("Content-Type", type);
		setSendFile(file.get());
	}

	return true;
}

void DataHandle::perform(Application *app, DataCompleteCallback &&cb) {
	if (!cb) {
		return Handle::perform(app, nullptr);
	} else {
		return Handle::perform(app, [this, cb = move(cb)] (Handle &handle) {
			data::Value data;
			if (!_data.empty()) {
				data = data::read(_data);
			}
			cb(handle, data);
		});
	}
}

bool DataHandle::prepare(Context *ctx, const Callback<bool(CURL *)> &onBeforePerform) {
	bool hasAccept = false;
	for (auto &it : _sendedHeaders) {
		auto l = string::tolower(StringView(it).sub(0, "Accept:"_len));
		if (StringView(l).starts_with("accept:")) {
			hasAccept = true;
		}
	}
	if (!hasAccept) {
		ctx->headers = curl_slist_append(ctx->headers, "Accept: application/cbor, application/json");
	}

	setReceiveCallback([&] (char *ptr, size_t size) -> size_t {
		if (_data.empty()) {
			if (auto size = getReceivedHeaderInt("Content-Length")) {
				_data.resize(size);
			}
		}
		if (_data.size() < _offset + size) {
			_data.resize(_offset + size);
		}

		auto target = _data.data() + _offset;
		memcpy(target, ptr, size);
		_offset += size;
		return size;
	});

	return Handle::prepare(ctx, onBeforePerform);
}

AssetHandle::~AssetHandle() { }

bool AssetHandle::init(Rc<storage::Asset> &&, StringView tmp) {
	// TODO
	return true;
}

Rc<storage::Asset> AssetHandle::getAsset() const {
	return _asset;
}

}
