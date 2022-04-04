/**
Copyright (c) 2020-2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_CORE_XLFORWARD_H_
#define XENOLITH_CORE_XLFORWARD_H_

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPThreadTask.h"
#include "SPData.h"
#include "SPLog.h"
#include "SPSpanView.h"
#include <optional>

#include "XLConfig.h"

#define XL_ASSERT(cond, msg)  do { if (!(cond)) { stappler::log::text("Assert", msg); } assert((cond)); } while (0)

#ifndef XLASSERT
#if DEBUG
#define XLASSERT(cond, msg) XL_ASSERT(cond, msg)
#else
#define XLASSERT(cond, msg)
#endif
#endif

#include "XLVec2.h"
#include "XLVec3.h"
#include "XLVec4.h"
#include "XLGeometry.h"
#include "XLQuaternion.h"
#include "XLMat4.h"
#include "XLPadding.h"
#include "XLColor.h"
#include "XLMovingAverage.h"
#include "XLFontStyle.h"

// GL thread loop debug (uncomment to enable)
// #define XL_LOOP_DEBUG 1

// RenderGraph/Frame debug (uncomment to enable)
// #define XL_FRAME_DEBUG 1

// General Vulkan debug (uncomment to enable)
// #define XL_VK_DEBUG 1

// Enable Vulkan function call hooks (uncomment to enable)
// #define XL_VK_HOOK_DEBUG 1

// based on VK_MAKE_API_VERSION
#define XL_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

namespace stappler::xenolith {

using FilePath = ValueWrapper<StringView, class FilePathTag>;

enum ScreenOrientation {
	Landscape = 1,
	LandscapeLeft,
	LandscapeRight,
	Portrait,
	PortraitTop,
	PortraitBottom
};

enum class RenderingLevel {
	Default,
	Solid,
	Surface,
	Transparent
};

class Event;
class EventHeader;
class EventHandler;
class EventHandlerNode;

class Application;
class Director;

using Task = thread::Task;

template <typename Callback>
inline auto perform(const Callback &cb, memory::pool_t *p) {
	struct Context {
		Context(memory::pool_t *p) {
			memory::pool::push(p);
		}
		~Context() {
			memory::pool::pop();
		}

		memory::pool_t *pool = nullptr;
	} holder(p);
	return cb();
}

template <typename T>
inline bool exists_ordered(memory::vector<T> &vec, const T & val) {
	auto lb = std::lower_bound(vec.begin(), vec.end(), val);
	if (lb == vec.end() || *lb != val) {
		return false;
	}
	return true;
}

} // stappler::xenolith


namespace stappler::xenolith::network {

class Controller;

}


namespace stappler::xenolith::storage {

class StorageRoot;
class Server;
class Asset;
class AssetLibrary;

}

namespace stappler::xenolith::font {

class FontLibrary;
class FontController;
class FontFaceObject;

}

#endif /* XENOLITH_CORE_XLFORWARD_H_ */
