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

#include "XLAsset.h"
#include "XLAssetLibrary.h"
#include "XLNetworkRequest.h"
#include "XLApplication.h"

#include "curl/curl.h"

namespace stappler::xenolith::storage {

Asset::Asset(AssetLibrary *lib, const db::Value &val) : _library(lib) {
	bool resumeDownload = false;
	const db::Value *versions = nullptr;
	for (auto &it : val.asDict()) {
		if (it.first == "__oid") {
			_id = reinterpretValue<uint64_t>(it.second.getInteger());
		} else if (it.first == "url") {
			_url = StringView(it.second.getString()).str<Interface>();
		} else if (it.first == "data") {
			_data = Value(it.second);
		} else if (it.first == "mtime") {
			_mtime = Time(it.second.getInteger());
		} else if (it.first == "touch") {
			_touch = Time(it.second.getInteger());
		} else if (it.first == "ttl") {
			_ttl = TimeInterval(it.second.getInteger());
		} else if (it.first == "download") {
			resumeDownload = it.second.getBool();
		} else if (it.first == "versions") {
			versions = &it.second;
		}
	}

	_path = AssetLibrary::getAssetPath(_id);
	_cache = toString(_path, "/cache");

	filesystem::mkdir(_path);
	filesystem::mkdir(_cache);

	if (versions) {
		parseVersions(*versions);
	}

	if (resumeDownload) {
		download();
	}
}

Asset::~Asset() {
	_library->removeAsset(this);
}

bool Asset::download() {
	if (_download) {
		return true;
	}

	auto it = _versions.begin();
	while (it != _versions.end()) {
		if (!it->complete) {
			if (resumeDownload(*it)) {
				return true;
			} else {
				filesystem::remove(it->path, true, true);
				_library->eraseVersion(it->id);
				it = _versions.erase(it);
			}
		} else {
			++ it;
		}
	}

	if (!_versions.empty()) {
		if (filesystem::exists(_versions.front().path)) {
			return startNewDownload(_versions.front().ctime, _versions.front().etag);
		}
	}

	return startNewDownload(Time(), StringView());
}

void Asset::touch(Time t) {
	_touch = t;
	_dirty = true;
}

void Asset::clear() {
	auto it = _versions.begin();
	while (it != _versions.end()) {
		if (it->complete) {
			filesystem::remove(it->path, true, true);
			_library->eraseVersion(it->id);
			it = _versions.erase(it);
		} else {
			++ it;
		}
	}
}

bool Asset::isDownloadInProgress() const {
	return _download;
}

float Asset::getProgress() const {
	for (auto &it : _versions) {
		if (it.id == _downloadId) {
			return it.progress;
		}
	}
	return _versions.empty() ? 0.0f : (_versions.front().complete ? 1.0f : 0.0f);
}

void Asset::setData(const Value &d) {
	_data = d;
	_dirty = true;
}

void Asset::setData(Value &&d) {
	_data = std::move(d);
	_dirty = true;
}

Value Asset::encode() const {
	return Value({
		pair("ttl", Value(_ttl.toMicros())),
		pair("touch", Value(_touch.toMicros())),
		pair("data", Value(_data)),
	});
}

void Asset::parseVersions(const db::Value &downloads) {
	Set<String> paths;
	Set<String> pathsToRemove;

	for (auto &download : downloads.asArray()) {
		VersionData data;
		for (auto &it : download.asDict()) {
			if (it.first == "__oid") {
				data.id = it.second.getInteger();
			} else if (it.first == "etag") {
				data.etag = StringView(it.second.getString()).str<Interface>();
			} else if (it.first == "ctime") {
				data.ctime = Time(it.second.getInteger());
			} else if (it.first == "mtime") {
				data.mtime = Time(it.second.getInteger());
			} else if (it.first == "size") {
				data.size = Time(it.second.getInteger());
			} else if (it.first == "type") {
				data.contentType = StringView(it.second.getString()).str<Interface>();
			} else if (it.first == "complete") {
				data.complete = it.second.getBool();
			}
		}

		auto tag = StringView(data.etag);
		tag.trimChars<StringView::Chars<'"', '\'', ' ', '-'>>();
		auto versionPath = toString(_path, "/", data.ctime.toMicros(), "-", tag);
		auto iit = paths.find(versionPath);
		if (iit != paths.end()) {
			_library->eraseVersion(data.id);
		} else {
			if (filesystem::exists(versionPath)) {
				auto &v = _versions.emplace_back(move(data));
				v.path = move(versionPath);
				v.isFile = true;
				v.download = true;

				paths.emplace(v.path);
			} else {
				_library->eraseVersion(data.id);
			}
		}
	}

	filesystem::ftw(_path, [&] (StringView path, bool isFile) {
		if (!isFile && path != _cache && path != _path) {
			auto it = paths.find(path);
			if (it == paths.end()) {
				pathsToRemove.emplace(path.str<Interface>());
			}
		}
	}, 1);

	for (auto &it : pathsToRemove) {
		filesystem::remove(it, true, true);
	}

	bool localFound = false;
	bool pendingFound = false;

	auto it = _versions.begin();
	while (it != _versions.end()) {
		if (it->complete) {
			if (!localFound) {
				localFound = true;
				++ it;
			} else {
				_library->eraseVersion(it->id);
				it = _versions.erase(it);
			}
		} else {
			if (!pendingFound) {
				pendingFound = true;
				++ it;
			} else {
				_library->eraseVersion(it->id);
				it = _versions.erase(it);
			}
		}
	}
}

struct AssetDownloadData : Ref {
	Rc<Asset> asset;
	Asset::VersionData data;
	FILE *inputFile = nullptr;
	bool valid = true;
	float progress = 0.0f;

	AssetDownloadData(Rc<Asset> &&a)
	: asset(move(a)) { }

	AssetDownloadData(Rc<Asset> &&a, Asset::VersionData &data)
	: asset(move(a)), data(data) { }
};

bool Asset::startNewDownload(Time ctime, StringView etag) {
	auto data = Rc<AssetDownloadData>::alloc(this);

	auto req = Rc<network::Request>::create([&] (network::Handle &handle) {
		handle.init(network::Method::Get, _url);

		handle.setMTime(ctime.toMicros());
		handle.setETag(etag);

		handle.setHeaderCallback([data = data.get()] (StringView key, StringView value) {
			if (key == "last-modified") {
				data->data.ctime = std::max(Time::fromHttp(value), data->data.ctime);
			} else if (key == "x-filemodificationtime") {
				if (uint64_t v = value.readInteger(10).get(0)) {
					data->data.ctime = std::max(Time::microseconds(v), data->data.ctime);
				}
			} else if (key == "etag") {
				data->data.etag = value.str<Interface>();
			} else if (key == "content-length") {
				data->data.size = std::max(size_t(value.readInteger(10).get(0)), data->data.size);
			} else if (key == "x-filesize") {
				data->data.size = std::max(size_t(value.readInteger(10).get(0)), data->data.size);
			} else if (key == "content-type") {
				data->data.contentType = value.str<Interface>();
			}
		});
		handle.setReceiveCallback([this, data = data.get()] (char *bytes, size_t size) {
			if (!data->valid) {
				return size_t(CURL_WRITEFUNC_ERROR);
			}

			if (!data->inputFile) {
				auto tag = StringView(data->data.etag);
				tag.trimChars<StringView::Chars<'"', '\'', ' ', '-'>>();
				data->data.path = toString(_path, "/", data->data.ctime.toMicros(), "-", tag);
				data->inputFile = fopen(data->data.path.data(), "w");
				if (!data->inputFile) {
					return size_t(CURL_WRITEFUNC_ERROR);
				}
				addVersion(data);
			}

			return size_t(fwrite(bytes, size, 1, data->inputFile) * size);
		});
		return true;
	}, data);

	req->setDownloadProgress([this, data = data.get()] (const network::Request &, int64_t total, int64_t now) {
		data->progress = float(now) / float(total);
		setDownloadProgress(data->data.id, data->progress);
	});

	_download = true;
	_library->setAssetDownload(_id, _download);

	req->perform(_library->getApplication(), [this, data = data.get()] (const network::Request &req, bool success) {
		std::cout << req.getHandle().getDebugData().str() << "\n";
		if (data->inputFile) {
			fclose(data->inputFile);
			data->inputFile = nullptr;

			setDownloadComplete(data->data, data->valid && success);
			return;
		} else {
			auto code = req.getHandle().getResponseCode();
			if (code >= 300 && code < 400) {
				setFileValidated(success);
				return;
			}
		}

		setDownloadComplete(data->data, data->valid && false);
	});
	return true;
}

bool Asset::resumeDownload(VersionData &d) {
	filesystem::Stat stat;
	if (!filesystem::stat(d.path, stat)) {
		return false;
	}

	auto data = Rc<AssetDownloadData>::alloc(this, d);

	auto req = Rc<network::Request>::create([&] (network::Handle &handle) {
		handle.init(network::Method::Get, _url);

		handle.setResumeOffset(stat.size);
		handle.setHeaderCallback([data = data.get()] (StringView key, StringView value) {
			if (key == "last-modified") {
				if (Time::fromHttp(value) > data->data.ctime) {
					data->valid = false;
				}
			} else if (key == "etag") {
				if (data->data.etag != value) {
					data->valid = false;
				}
			}
		});
		handle.setReceiveCallback([data = data.get()] (char *bytes, size_t size) {
			if (!data->valid) {
				return size_t(CURL_WRITEFUNC_ERROR);
			}

			if (!data->inputFile) {
				data->inputFile = fopen(data->data.path.data(), "a");
				if (!data->inputFile) {
					return size_t(CURL_WRITEFUNC_ERROR);
				}
			}

			return size_t(fwrite(bytes, size, 1, data->inputFile) * size);
		});
		return true;
	}, data);

	req->setDownloadProgress([this, data = data.get()] (const network::Request &, int64_t total, int64_t now) {
		data->progress = float(now) / float(total);
		setDownloadProgress(data->data.id, data->progress);
	});

	_downloadId = d.id;
	_download = true;
	_library->setAssetDownload(_id, _download);

	req->perform(_library->getApplication(), [this, data = data.get()] (const network::Request &req, bool success) {
		if (data->inputFile) {
			fclose(data->inputFile);
			data->inputFile = nullptr;
		}

		setDownloadComplete(data->data, data->valid && success);
	});
	return true;
}

void Asset::setDownloadProgress(int64_t id, float progress) {
	for (auto &it : _versions) {
		if (it.id == id) {
			it.progress = progress;
			setDirty(Flags(Update::DownloadProgress));
		}
	}
}

void Asset::setDownloadComplete(VersionData &data, bool success) {
	data.complete = success;

	_download = false;
	_library->setAssetDownload(_id, _download);

	if (success) {
		for (auto &it : _versions) {
			if (it.id == data.id) {
				replaceVersion(data);
				setDirty(Flags(Update::DownloadCompleted));
				_library->setVersionComplete(data.id, true);
				return;
			}
		}
	} else {
		auto it = _versions.begin();
		while (it != _versions.end()) {
			if (it->id == data.id) {
				_library->eraseVersion(it->id);
				filesystem::remove(it->path, true, true);
				it = _versions.erase(it);
				setDirty(Flags(Update::DownloadFailed));
			} else {
				++ it;
			}
		}
	}

	_downloadId = 0;
}

void Asset::setFileValidated(bool success) {
	_download = false;
	_library->setAssetDownload(_id, _download);
	_downloadId = 0;
}

void Asset::replaceVersion(VersionData &data) {
	for (auto &it : _versions) {
		if (it.id != data.id) {
			filesystem::remove(it.path, true, true);
			_library->eraseVersion(it.id);
		}
	}

	_versions.clear();
	_versions.emplace_back(data);
	_touch = Time::now();
}

void Asset::addVersion(AssetDownloadData *data) {
	_library->perform([this, data] (const Server &, const db::Transaction &t) {
		auto id = _library->addVersion(t, _id, data->data);
		_library->getApplication()->performOnMainThread([this, id, data] {
			_downloadId = data->data.id = id;
			_versions.emplace_back(data->data);
			setDirty(Flags(Update::DownloadStarted));
		}, data);
		return true;
	}, data);
}

}
