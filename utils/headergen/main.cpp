/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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


#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPBitmap.h"
#include "RegistryData.h"

static constexpr auto HELP_STRING(
R"HelpString(headergen <options> registry|icons
Options:
    -v (--verbose)
    -h (--help))HelpString");

namespace stappler::xenolith::headergen {

int parseOptionSwitch(Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	}
	return 1;
}

int parseOptionString(Value &ret, const StringView &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.setBool(true, "help");
	} else if (str == "verbose") {
		ret.setBool(true, "verbose");
	} else if (str == "gencbor") {
		ret.setBool(true, "gencbor");
	}
	return 1;
}

SP_EXTERN_C int _spMain(argc, argv) {
	Value opts = data::parseCommandLineOptions<Interface>(argc, argv,
			&parseOptionSwitch, &parseOptionString);
	if (opts.getBool("help")) {
		std::cout << HELP_STRING << "\n";
		return 0;
	}

	if (opts.getBool("verbose")) {
		std::cout << " Current work dir: " << filesystem::currentDir<Interface>() << "\n";
		std::cout << " Options: " << data::EncodeFormat::Pretty << opts << "\n";
	}

	auto arg = opts.getValue("args").getString(1);

	if (arg.empty() || arg == "registry") {
		RegistryData registryData;
		if (registryData.load()) {
			registryData.write();
		}
	} else if (arg == "icons") {
		auto exportIcon = [] (StringView path) {
			auto name = filepath::name(path);
			auto target = filepath::merge<Interface>(filepath::root(path), toString(name, ".lzimg"));
			auto targetH = filepath::merge<Interface>(filepath::root(path), toString(name, ".h"));
			auto b = filesystem::readIntoMemory<Interface>(path);

			Bitmap bmp(b);
			bmp.convert(bitmap::PixelFormat::RGBA8888);

			std::cout << "Image: " << filepath::name(path) << ": " << bmp.width() << " x " << bmp.height() << "\n";

			Value val{
				pair("width", Value(bmp.width())),
				pair("height", Value(bmp.height())),
				pair("data", Value(bmp.data()))
			};

			data::save(val, target, data::EncodeFormat::CborCompressed);
			auto bytes = data::write(val, data::EncodeFormat::CborCompressed);

			size_t counter = 16;

			StringStream stream;
			stream << "icon = {" << std::hex;\
			for (auto &it : bytes) {
				++ counter;
				if (counter >= 16) {
					stream << "\n\t";
					counter = 0;
				} else {
					stream << " ";
				}
				stream << "0x" << uint32_t(it) << ",";
			}
			stream << "\n}\n";

			filesystem::write(targetH, stream.str());
		};

		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-close-symbolic.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-maximize-symbolic.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-minimize-symbolic.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-restore-symbolic.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-close-symbolic-active.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-maximize-symbolic-active.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-minimize-symbolic-active.png")));
		exportIcon(filepath::reconstructPath<Interface>(
				filesystem::currentDir<Interface>("../../resources/images/window-restore-symbolic-active.png")));
	}

	return 0;
}

}
