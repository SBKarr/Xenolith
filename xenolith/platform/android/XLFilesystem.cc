/**
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

#include "XLPlatform.h"
#include "SPFilesystem.h"

#if ANDROID

#include <android/asset_manager.h>

namespace stappler::filesystem::platform {

struct PathSource {
	memory::StandartInterface::StringType _appPath;
	memory::StandartInterface::StringType _cachePath;
	memory::StandartInterface::StringType _documentsPath;
	memory::StandartInterface::StringType _writablePath;

	std::mutex _mutex;

	AAssetManager *_assetManager = nullptr;

	bool _cacheInit = false;
	bool _documentsInit = false;

	static PathSource *getInstance() {
		static std::mutex s_mutex;
		static PathSource *s_paths = nullptr;
		std::unique_lock lock(s_mutex);
		if (!s_paths) {
			s_paths = new PathSource;
		}
		return s_paths;
	}

	PathSource() { }

	bool initialize(AAssetManager *assetManager, StringView filesDir, StringView cachesDir) {
		_appPath = filesDir.str<memory::StandartInterface>();
		_writablePath = cachesDir.str<memory::StandartInterface>();
		_documentsPath = _writablePath + "/Documents";
		_cachePath = _writablePath + "/Caches";

		std::unique_lock lock(_mutex);
		_assetManager = assetManager;
		_documentsInit = false;
		_cacheInit = false;
		return true;
	}

	void terminate() {
		std::unique_lock lock(_mutex);
		_assetManager = nullptr;
	}

	StringView getApplicationPath() {
		return _appPath;
	}

	StringView getDocumentsPath(bool readOnly) {
		if (!readOnly) {
			if (!_documentsInit) {
				filesystem::mkdir(_documentsPath);
				_documentsInit = true;
			}
		}
		return _documentsPath;
	}
	StringView getCachePath(bool readOnly) {
		if (!readOnly) {
			if (!_cacheInit) {
				filesystem::mkdir(_cachePath);
				_cacheInit = true;
			}
		}
		return _cachePath;
	}
	StringView getWritablePath(bool readOnly) {
		return _writablePath;
	}

	StringView _getPlatformPath(StringView path) {
		if (filepath::isBundled(path)) {
			auto tmp = path.sub("%PLATFORM%:"_len);
			while (tmp.is('/')) {
				tmp = tmp.sub(1);
			}
			return tmp;
		}
		return path;
	}

	StringView _getAssetsPath(StringView ipath) {
		auto path = _getPlatformPath(ipath);
		if (path.starts_with("assets/")) {
			path = path.sub("assets/"_len);
		}

		return path;
	}

	bool exists(StringView ipath) { // check for existance in platform-specific filesystem
		if (ipath.empty() || ipath.front() == '/' || ipath.starts_with("..") || ipath.find("/..") != maxOf<size_t>()) {
			return false;
		}

		if (!_assetManager) {
			return false;
		}

		auto path = _getAssetsPath(ipath);

		AAsset* aa = AAssetManager_open(_assetManager,
				(path.terminated()?path.data():path.str<memory::StandartInterface>().data()), AASSET_MODE_UNKNOWN);
		if (aa) {
			AAsset_close(aa);
			return true;
		}

		AAssetDir *adir = AAssetManager_openDir(_assetManager,
				(path.terminated()?path.data():path.str<memory::StandartInterface>().data()));
		if (adir) {
			AAssetDir_close(adir);
			return true;
		}

		return false;
	}

	bool stat(StringView ipath, Stat &stat) {
		if (!_assetManager) {
			return false;
		}

		auto path = _getAssetsPath(ipath);

		AAsset* aa = AAssetManager_open(_assetManager,
				(path.terminated()?path.data():path.str<memory::StandartInterface>().data()), AASSET_MODE_UNKNOWN);
		if (aa) {
			stat.size = AAsset_getLength64(aa);
			stat.isDir = false;
			AAsset_close(aa);
			return true;
		}

		AAssetDir *adir = AAssetManager_openDir(_assetManager,
				(path.terminated()?path.data():path.str<memory::StandartInterface>().data()));
		if (adir) {
			stat.isDir = true;
			AAssetDir_close(adir);
			return true;
		}

		return false;
	}

	File openForReading(StringView ipath) {
		if (!_assetManager) {
			return false;
		}

		auto path = _getAssetsPath(ipath);

		AAsset* aa = AAssetManager_open(_assetManager,
				(path.terminated()?path.data():path.str<memory::StandartInterface>().data()), AASSET_MODE_UNKNOWN);
		if (aa) {
			auto len = AAsset_getLength64(aa);
			return File((void *)aa, len);
		}

		return File();
	}
};

void Android_initializeFilesystem(AAssetManager *assetManager, StringView filesDir, StringView cachesDir) {
	PathSource::getInstance()->initialize(assetManager, filesDir, cachesDir);
}

void Android_terminateFilesystem() {
	PathSource::getInstance()->terminate();
}

template <>
auto _getApplicationPath<memory::StandartInterface>() -> memory::StandartInterface::StringType {
	return StringView(PathSource::getInstance()->getApplicationPath()).str<memory::StandartInterface>();
}

template <>
auto _getApplicationPath<memory::PoolInterface>() -> memory::PoolInterface::StringType {
	return StringView(PathSource::getInstance()->getApplicationPath()).str<memory::PoolInterface>();
}

template <>
auto _getWritablePath<memory::StandartInterface>(bool readOnly) -> typename memory::StandartInterface::StringType {
	return PathSource::getInstance()->getWritablePath(readOnly).str<memory::StandartInterface>();
}

template <>
auto _getWritablePath<memory::PoolInterface>(bool readOnly) -> typename memory::PoolInterface::StringType {
	return StringView(PathSource::getInstance()->getWritablePath(readOnly)).str<memory::PoolInterface>();
}

template <>
auto _getDocumentsPath<memory::StandartInterface>(bool readOnly) -> typename memory::StandartInterface::StringType {
	return PathSource::getInstance()->getDocumentsPath(readOnly).str<memory::StandartInterface>();
}

template <>
auto _getDocumentsPath<memory::PoolInterface>(bool readOnly) -> typename memory::PoolInterface::StringType {
	return StringView(PathSource::getInstance()->getDocumentsPath(readOnly)).str<memory::PoolInterface>();
}

template <>
auto _getCachesPath<memory::StandartInterface>(bool readOnly) -> typename memory::StandartInterface::StringType {
	return PathSource::getInstance()->getCachePath(readOnly).str<memory::StandartInterface>();
}

template <>
auto _getCachesPath<memory::PoolInterface>(bool readOnly) -> typename memory::PoolInterface::StringType {
	return StringView(PathSource::getInstance()->getCachePath(readOnly)).str<memory::PoolInterface>();
}

bool _exists(StringView path) { // check for existance in platform-specific filesystem
	if (path.empty() || path.front() == '/' || path.starts_with("..") || path.find("/..") != maxOf<size_t>()) {
		return false;
	}

	return PathSource::getInstance()->exists(path);
}

bool _stat(StringView path, Stat &stat) {
	return PathSource::getInstance()->stat(path, stat);
}

File _openForReading(StringView path) {
	return PathSource::getInstance()->openForReading(path);
}

size_t _read(void *aa, uint8_t *buf, size_t nbytes) {
	auto r = AAsset_read((AAsset *)aa, buf, nbytes);
	if (r >= 0) {
		return r;
	}
	return 0;
}

size_t _seek(void *aa, int64_t offset, io::Seek s) {
	int whence = SEEK_SET;
	switch (s) {
	case io::Seek::Set: whence = SEEK_SET; break;
	case io::Seek::Current: whence = SEEK_CUR; break;
	case io::Seek::End: whence = SEEK_END; break;
	}
	if (auto r = AAsset_seek64((AAsset *)aa, off64_t(offset), whence) > 0) {
		return r;
	}
	return maxOf<size_t>();
}

size_t _tell(void *aa) {
	return AAsset_seek64((AAsset *)aa, off64_t(0), int(SEEK_CUR));
}

bool _eof(void *aa) {
	return AAsset_getRemainingLength64((AAsset *)aa) == 0;
}
void _close(void *aa) {
	AAsset_close((AAsset *)aa);
}

}

#endif
