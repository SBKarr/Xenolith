/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLNetworkController.h"
#include "XLApplication.h"
#include "SPNetworkContext.h"
#include "XLNetworkRequest.h"

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

/*void Handle::perform(Application *app, CompleteCallback &&cb) {
	if (cb) {
		_onComplete = move(cb);
	}
	if (!app) {
		app = Application::getInstance();
	}
	auto c = app->getNetworkController();
	c->run(this);
}*/

bool Handle::prepare(Context *ctx) {
	if (_mtime > 0) {
		ctx->headers = curl_slist_append(ctx->headers, toString("If-Modified-Since: ", Time::microseconds(_mtime).toHttp<Interface>()).data());
	}
	if (!_etag.empty()) {
		ctx->headers = curl_slist_append(ctx->headers, toString("If-None-Match: ", _etag).data());
	}

	if (!_sharegroup.empty() && ctx->share) {
		setCookieFile(filesystem::writablePath<Interface>(toString("network.", _controller->getName(), ".", _sharegroup, ".cookies")));
	}

	setUserAgent(_controller->getApplication()->getUserAgent());

	return true;
}

bool Handle::finalize(Context *ctx, bool ret) {
	_success = ctx->success;

	if (getResponseCode() < 300) {
		_mtime = Time::fromHttp(getReceivedHeaderString("Last-Modified")).toMicroseconds();
		_etag = getReceivedHeaderString("ETag").str<Interface>();
	} else {
		auto &f = getReceiveDataSource();
		if (auto str = std::get_if<String>(&f)) {
			filesystem::remove(*str);
		}
	}

	return ret;
}

Request::~Request() { }

bool Request::init(const Callback<bool(Handle &)> &setupCallback, Rc<Ref> &&ref) {
	_owner = move(ref);
	return setupCallback(_handle);
}

void Request::perform(Application *app, CompleteCallback &&cb) {
	if (cb) {
		_onComplete = move(cb);
	}
	if (!app) {
		app = Application::getInstance();
	}

	auto &c = app->getNetworkController();
	_handle._request = this;
	_handle._controller = c;

	_uploadProgress = pair(0, 0);
	_downloadProgress = pair(0, 0);

	auto &source = _handle.getReceiveDataSource();
	if (std::holds_alternative<std::monostate>(source)) {
		if (!_ignoreResponseData) {
			_targetHeaderCallback = _handle.getHeaderCallback();

			_handle.setHeaderCallback([this] (StringView key, StringView value) {
				handleHeader(key, value);
			});

			_handle.setReceiveCallback([this] (char *buf, size_t size) -> size_t {
				return handleReceive(buf, size);
			});
		}
	}
	c->run(this);
}

void Request::setIgnoreResponseData(bool value) {
	if (!_running) {
		_ignoreResponseData = value;
	}
}

void Request::setUploadProgress(ProgressCallback &&cb) {
	_onUploadProgress = move(cb);
}

void Request::setDownloadProgress(ProgressCallback &&cb) {
	_onDownloadProgress = move(cb);
}

void Request::handleHeader(StringView key, StringView value) {
	if (!_ignoreResponseData) {
		if (key == "content-length") {
			auto length = value.readInteger(10).get(0);
			_data.resize(size_t(length));
		}
	}
	if (_targetHeaderCallback) {
		_targetHeaderCallback(key, value);
	}
}

size_t Request::handleReceive(char *buf, size_t nbytes) {
	if (_data.size() < (nbytes + _nbytes)) {
		_data.resize(nbytes + _nbytes);
	}
	memcpy(_data.data() + _nbytes, buf, nbytes);
	_nbytes += nbytes;
	return nbytes;
}

void Request::notifyOnComplete() {
	if (_onComplete) {
		_onComplete(*this);
	}
	_running = false;
	_handle._request = nullptr;
}

void Request::notifyOnUploadProgress(int64_t total, int64_t now) {
	_uploadProgress = pair(total, std::max(now, _uploadProgress.second)); // prevent out-of-order updates
	if (_onUploadProgress && _uploadProgress.second == now) {
		_onUploadProgress(*this, total, now);
	}
}

void Request::notifyOnDownloadProgress(int64_t total, int64_t now) {
	_downloadProgress = pair(total, std::max(now, _downloadProgress.second)); // prevent out-of-order updates
	if (_onDownloadProgress && _downloadProgress.second == now) {
		_onDownloadProgress(*this, total, now);
	}
}

/*bool DataHandle::init(StringView url, const data::Value &data, data::EncodeFormat format) {
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
}*/

}
