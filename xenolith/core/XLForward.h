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

#ifndef COMPONENTS_XENOLITH_CORE_XLFORWARD_H_
#define COMPONENTS_XENOLITH_CORE_XLFORWARD_H_

#include "SPLayout.h"
#include "SPFilesystem.h"
#include "SPThreadTask.h"
#include "SPData.h"
#include "SPLog.h"
#include "SPSpanView.h"

#include "SLFont.h"

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

using Vec2 = layout::Vec2;
using Vec3 = layout::Vec3;
using Vec4 = layout::Vec4;
using Size = layout::Size;
using Rect = layout::Rect;

using Size2 = layout::Size2;
using Size3 = layout::Size3;
using Extent2 = layout::Extent2;
using Extent3 = layout::Extent3;

using Mat4 = layout::Mat4;
using Quaternion = layout::Quaternion;

using Color = layout::Color;
using Color4B = layout::Color4B;
using Color4F = layout::Color4F;
using Color3B = layout::Color3B;

using Padding = layout::Padding;
using Margin = layout::Margin;

struct URect {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
};

template <size_t Count>
using MovingAverage = layout::MovingAverage<Count>;
using FilePath = layout::FilePath;

namespace Anchor {

constexpr const Vec2 Middle(0.5f, 0.5f);
constexpr const Vec2 BottomLeft(0.0f, 0.0f);
constexpr const Vec2 TopLeft(0.0f, 1.0f);
constexpr const Vec2 BottomRight(1.0f, 0.0f);
constexpr const Vec2 TopRight(1.0f, 1.0f);
constexpr const Vec2 MiddleRight(1.0f, 0.5f);
constexpr const Vec2 MiddleLeft(0.0f, 0.5f);
constexpr const Vec2 MiddleTop(0.5f, 1.0f);
constexpr const Vec2 MiddleBottom(0.5f, 0.0f);

}

enum ScreenOrientation {
	Landscape = 1,
	LandscapeLeft,
	LandscapeRight,
	Portrait,
	PortraitTop,
	PortraitBottom
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

using Metrics = layout::Metrics;
using CharLayout = layout::CharLayout;

enum FontAnchor : uint32_t {
	BottomLeft,
	TopLeft,
	TopRight,
	BottomRight
};

struct FontCharLayout {
	static uint32_t getObjectId(uint16_t sourceId, char16_t, FontAnchor);
	static uint32_t getObjectId(uint32_t, FontAnchor);

	CharLayout layout;
	uint16_t width;
	uint16_t height;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_XLFORWARD_H_ */
