/**
 Copyright (c) 2020-2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "SPVec2.h"
#include "SPVec3.h"
#include "SPVec4.h"
#include "SPGeometry.h"
#include "SPQuaternion.h"
#include "SPMat4.h"
#include "SPPadding.h"
#include "SPColor.h"


#if __CDT_PARSER__
// IDE-specific definition

#define MODULE_XENOLITH_STORAGE 1
#define MODULE_XENOLITH_NETWORK 1

#endif


// based on VK_MAKE_API_VERSION
#define XL_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

namespace stappler::xenolith {

// Import std memory model as default
using namespace stappler::mem_std;

using Vec2 = geom::Vec2;
using Vec3 = geom::Vec3;
using Vec4 = geom::Vec4;
using Mat4 = geom::Mat4;
using Size2 = geom::Size2;
using Size3 = geom::Size3;
using Extent2 = geom::Extent2;
using Extent3 = geom::Extent3;
using Rect = geom::Rect;
using URect = geom::URect;
using UVec2 = geom::UVec2;
using Quaternion = geom::Quaternion;
using Color = geom::Color;
using Color3B = geom::Color3B;
using Color4B = geom::Color4B;
using Color4F = geom::Color4F;
using ColorMask = geom::ColorMask;
using Padding = geom::Padding;
namespace Anchor = geom::Anchor;

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

class Node;
class Sprite;
class Label;
class Layer;

class Action;
class ActionInterval;
class ActionInstant;
class ActionProgress;

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

#if MODULE_XENOLITH_NETWORK
namespace stappler::xenolith::network {

class Controller;

}
#endif


#if MODULE_XENOLITH_STORAGE
namespace stappler::xenolith::storage {

class StorageRoot;
class Server;
class Asset;
class AssetLibrary;

}
#endif

namespace stappler::xenolith::font {

class FontLibrary;
class FontController;
class FontFaceObject;

}

#endif /* XENOLITH_CORE_XLFORWARD_H_ */
