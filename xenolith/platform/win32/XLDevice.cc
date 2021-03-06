/**
Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#if (CYGWIN || MSYS)

#include <wincrypt.h>

class RandomSequence {
	HCRYPTPROV hProvider;
public:
	RandomSequence(void) : hProvider(0) {
		if (FALSE == CryptAcquireContext(&hProvider, NULL, NULL, PROV_RSA_FULL, 0)) {
			// failed, should we try to create a default provider?
			if (NTE_BAD_KEYSET == HRESULT(GetLastError())) {
				if (FALSE == CryptAcquireContext(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
					// ensure the provider is NULL so we could use a backup plan
					hProvider = 0;
				}
			}
		}
	}

	~RandomSequence(void) {
		if (0 != hProvider) {
			CryptReleaseContext(hProvider, 0U);
		}
	}

	BOOL generate(BYTE *buf, DWORD len) {
		if (0 != hProvider) {
			return CryptGenRandom(hProvider, len, buf);
		}
		return FALSE;
	}
};

namespace stappler::xenolith::platform::device {
	bool _isTablet() {
		return desktop::isTablet();
	}
	String _userAgent() {
		return "Mozilla/5.0 (Windows;)";
	}
	String _deviceIdentifier() {
		auto path = stappler::platform::filesystem::_getCachesPath();
		auto devIdPath = path + "/.devid";
		if (stappler::filesystem::exists(devIdPath)) {
			auto data = stappler::filesystem::readIntoMemory(devIdPath);
			return base16::encode(data);
		} else {
			RandomSequence rnd;
			Bytes data; data.resize(16);
			if (!rnd.generate(data.data(), 16)) {
				log::text("Device", "Fail to read random bytes");
			} else {
				stappler::filesystem::write(devIdPath, data);
			}
			return base16::encode(data);
		}
	}
	String _bundleName() {
		return desktop::getPackageName();
	}
	String _userLanguage() {
		return desktop::getUserLanguage();
	}
	String _applicationName() {
		return "Linux App";
	}
	String _applicationVersion() {
		return desktop::getAppVersion();
	}
	ScreenOrientation _currentDeviceOrientation() {
		auto size = desktop::getScreenSize();
		if (size.width > size.height) {
			return ScreenOrientation::LandscapeLeft;
		} else {
			return ScreenOrientation::PortraitTop;
		}
	}
	Pair<uint64_t, uint64_t> _diskSpace() {
		uint64_t totalSpace = 0;
		uint64_t totalFreeSpace = 0;

		return pair(totalSpace, totalFreeSpace);
	}
	int _dpi() {
		return 92 * desktop::getDensity();
	}

	void _onDirectorStarted() { }
	float _density() {
		return desktop::getDensity();
	}
	void _sleep(double v) {
		Sleep(uint32_t(ceilf(v * 1000)));
	}
}

#endif
