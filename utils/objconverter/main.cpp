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

#include "../../modules/objfile/XLObjBundleFileBase.h"
#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPData.h"
#include "SPValid.h"

#include "XLObjFile.h"

static constexpr auto HELP_STRING(
R"HelpString(embedder --input <filename> --output <filename> --name <name>
Options:
    -v (--verbose)
    -h (--help)
	-c (--compress))HelpString");

namespace stappler::xenolith::objconverter {

using namespace stappler::mem_std;

static int parseOptionSwitch(Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	} else if (c == 'f') {
		ret.setBool(true, "force");
	}
	return 1;
}

static int parseOptionString(Value &ret, const StringView &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.setBool(true, "help");
	} else if (str == "verbose") {
		ret.setBool(true, "verbose");
	} else if (str == "force") {
		ret.setBool(true, "force");
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

	auto path = filesystem::currentDir<Interface>("cube.obj");
	if (filesystem::exists(path)) {
		auto objfile = Rc<obj::ObjFile>::create(FilePath(path));
		if (objfile) {
			auto bundlePath = filesystem::currentDir<Interface>("cube.bundle");
			auto bundle = Rc<obj::ObjBundleFileBase>::create();
			bundle->addObject(*objfile);
			filesystem::remove(bundlePath);
			bundle->save(FilePath(bundlePath));

			auto newBundle = Rc<obj::ObjBundleFileBase>::create(FilePath(bundlePath));

			return 0;
		}
	}

	return -1;
}

}
