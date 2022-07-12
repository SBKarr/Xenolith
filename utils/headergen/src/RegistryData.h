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

#ifndef UTILS_HEADERGEN_SRC_REGISTRYDATA_H_
#define UTILS_HEADERGEN_SRC_REGISTRYDATA_H_

#include "SPString.h"
#include "SPMemory.h"

namespace stappler::xenolith::headergen {

using namespace stappler::mem_std;

struct VkRegistryCommandProto {
	Vector<StringView> strings;
	StringView type;
	StringView name;
};

struct VkRegistryCommand {
	Vector<StringView> successcodes;
	Vector<StringView> errorcodes;

	VkRegistryCommandProto proto;
	Vector<VkRegistryCommandProto> params;

	StringView name;
	StringView alias;

	VkRegistryCommand *canonical = nullptr;
	Vector<StringView> aliases;

	StringView rootType;
	Vector<Pair<StringView, StringView>> _requires;
	String guard;
};

struct VkRegistryType {
	StringView alias;
	StringView name;
	StringView parent;
	StringView category;

	VkRegistryType *parentType = nullptr;
	VkRegistryType *canonical = nullptr;
	Vector<StringView> aliases;
};

struct VkRegistryRequire {
	StringView extension;
	StringView feature;
	Vector<StringView> commands;
};

struct VkRegistryFeature {
	StringView name;
	Vector<VkRegistryRequire> _requires;
};

struct VkRegistryExtension {
	StringView name;
	StringView type;
	StringView supported;
	Vector<StringView> _requires;
	Vector<VkRegistryRequire> commands;
};

struct RegistryData {
	bool load();
	StringView getRootType(VkRegistryCommand &cmd);
	VkRegistryCommand *getCommand(StringView cmd);
	void parse();

	String makeGuard(StringView name, SpanView<Pair<StringView, StringView>> reqs);
	void writeCommandFields(std::ostream &out, StringView guard, SpanView<StringView> commands);
	void writeLoaderConstructor(std::ostream &out, StringView guard, SpanView<StringView> commands);
	void writeInstanceConstructor(std::ostream &out, StringView guard, SpanView<StringView> commands);
	void writeDeviceConstructor(std::ostream &out, StringView guard, SpanView<StringView> commands);
	void writeHooks(std::ostream &out, StringView guard, SpanView<StringView> commands, StringView ctx);
	void writeHooksAddr(std::ostream &out, StringView guard, SpanView<StringView> commands, StringView ctx);
	void write();

	Map<StringView, VkRegistryType> types;
	Map<StringView, VkRegistryCommand> commands;

	Vector<VkRegistryFeature> features;
	Vector<VkRegistryExtension> extensions;

	Vector<StringView> loaderCommands;
	Vector<StringView> instanceCommands;
	Vector<StringView> deviceCommands;

	memory::ostringstream data;
};

}

#endif /* UTILS_HEADERGEN_SRC_REGISTRYDATA_H_ */
