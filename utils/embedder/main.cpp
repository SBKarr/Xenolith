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
#include "SPData.h"
#include "SPValid.h"

static constexpr auto HELP_STRING(
R"HelpString(embedder --input <filename> --output <filename> --name <name>
Options:
    -v (--verbose)
    -h (--help)
	-c (--compress)
	-f (--force))HelpString");

namespace stappler::xenolith::embedder {

using namespace stappler::mem_std;

static int parseOptionSwitch(Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	} else if (c == 'f') {
		ret.setBool(true, "force");
	} else if (c == 'c') {
		ret.setBool(true, "compress");
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
	} else if (str == "compress") {
		ret.setBool(true, "compress");
	} else if (str == "input") {
		ret.setString(StringView(*argv), "input");
		return 2;
	} else if (str == "output" && argc >= 1) {
		ret.setString(StringView(*argv), "output");
		return 2;
	} else if (str == "name" && argc >= 1) {
		ret.setString(StringView(*argv), "name");
		return 2;
	}
	return 1;
}

auto LICENSE_STRING =
R"Text(/**
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

// Generated with embedder

)Text";

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

	if (!opts.isString("input")) {
		std::cerr << "missed --input <filename>\n";
		return -1;
	}
	if (!opts.isString("output")) {
		std::cerr << "missed --output <filename>\n";
		return -1;
	}
	if (!opts.isString("name")) {
		std::cerr << "missed --name <varname>\n";
		return -1;
	}

	bool force = opts.getBool("force");
	auto name = opts.getString("name");
	auto output = opts.getString("output");
	auto input = opts.getString("input");
	if (!valid::validateIdentifier(name)) {
		std::cerr << "name '" << name << "' is not valid c identifier\n";
		return -1;
	}

	if (!filesystem::exists(input)) {
		if (filesystem::exists(filesystem::currentDir<Interface>(input))) {
			input = filesystem::currentDir<Interface>(input);
			if (!filepath::isAbsolute(output)) {
				output = filesystem::currentDir<Interface>(output);
			}
		} else {
			std::cerr << "Input file '" << input << "' not exists\n";
			return -1;
		}
	}

	if (filesystem::exists(output)) {
		if (force) {
			filesystem::remove(output);
			if (filesystem::exists(output)) {
				std::cerr << "Output file '" << output << "' exists\n";
				return -1;
			}
		} else {
			std::cerr << "Output file '" << output << "' exists (use -f to override)\n";
			return -1;
		}
	}

	auto data = filesystem::readIntoMemory<Interface>(input);

	if (opts.getBool("compress")) {
		data = data::compress<Interface>(data.data(), data.size(), data::EncodeFormat::Compression::LZ4HCCompression, false);
	}

	if (data.size() % sizeof(uint32_t) == 0) {
		auto view = makeSpanView((uint32_t *)data.data(), data.size() / sizeof(uint32_t));

		std::stringstream stream;
		stream << LICENSE_STRING << "#pragma once\nconst uint32_t " << name << "[] = {";
		size_t idx = 0;
		for (auto &it : view) {
			if (idx % 8 == 0) {
				stream << "\n\t";
			}
			stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << it << ",";
			++ idx;
		}

		stream << "\n};\n";

		filesystem::write(output, stream.str());

	} else {
		std::cerr << "Input stride is invalid: " << data.size() % sizeof(uint32_t) << "\n";
		return -1;
	}

	return 0;
}

}
