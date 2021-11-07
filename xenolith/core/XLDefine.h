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

#ifndef COMPONENTS_XENOLITH_CORE_XLDEFINE_H_
#define COMPONENTS_XENOLITH_CORE_XLDEFINE_H_

#include "XLForward.h"
#include "XLGlEnum.h"

namespace stappler::xenolith {

static constexpr uint64_t InvalidTag = maxOf<uint64_t>();

constexpr uint64_t operator"" _usec ( unsigned long long int val ) { return val * 1000'000; }
constexpr uint64_t operator"" _umsec ( unsigned long long int val ) { return val * 1000; }
constexpr uint64_t operator"" _umksec ( unsigned long long int val ) { return val; }

enum class NodeFlags {
	None,
	TransformDirty = 1 << 0,
	ContentSizeDirty = 1 << 1,

	DirtyMask = TransformDirty | ContentSizeDirty
};

struct ColorMode {
	enum Mode {
		Solid,
		Custom
	};

	uint32_t mode : 4;
	uint32_t r : 7;
	uint32_t g : 7;
	uint32_t b : 7;
	uint32_t a : 7;

	ColorMode() : mode(Solid), r(0), g(0), b(0), a(0) { }
	ColorMode(gl::ComponentMapping r, gl::ComponentMapping g, gl::ComponentMapping b, gl::ComponentMapping a)
	: mode(Custom), r(toInt(r)), g(toInt(g)), b(toInt(b)), a(toInt(a)) { }

	ColorMode(const ColorMode &) = default;
	ColorMode(ColorMode &&) = default;
	ColorMode &operator=(const ColorMode &) = default;
	ColorMode &operator=(ColorMode &&) = default;

	bool operator==(const ColorMode &n) const {
		return memcmp(this, &n, sizeof(ColorMode)) == 0;
	}

	bool operator!=(const ColorMode &n) const {
		return memcmp(this, &n, sizeof(ColorMode)) != 0;
	}

	Mode getMode() const { return Mode(mode); }
	gl::ComponentMapping getR() const { return gl::ComponentMapping(r); }
	gl::ComponentMapping getG() const { return gl::ComponentMapping(g); }
	gl::ComponentMapping getB() const { return gl::ComponentMapping(b); }
	gl::ComponentMapping getA() const { return gl::ComponentMapping(a); }
};

SP_DEFINE_ENUM_AS_MASK(NodeFlags)

namespace AppEvent {
	using Value = uint32_t;

	constexpr uint32_t None = 0;
	constexpr uint32_t Terminate = 1;
	constexpr uint32_t SwapchainRecreation = 2;
	constexpr uint32_t SwapchainRecreationBest = 4;
	constexpr uint32_t Update = 8;
	constexpr uint32_t Thread = 16;
	constexpr uint32_t Input = 32;
}

static constexpr auto EmptyTextureName = "org.xenolith.EmptyImage";
static constexpr auto SolidTextureName = "org.xenolith.SolidImage";

struct MaterialInfo {
	std::array<uint64_t, config::MaxMaterialImages> images = { 0 };
	std::array<uint16_t, config::MaxMaterialImages> samplers = { 0 };
	gl::MaterialType type = gl::MaterialType::Basic2D;
	ColorMode colorMode;

	uint64_t hash() const {
		return hash::hash64((const char *)this, sizeof(MaterialInfo));
	}

	bool operator==(const MaterialInfo &info) const {
		return memcmp(this, &info, sizeof(MaterialInfo)) == 0;
	}

	bool operator!=(const MaterialInfo &info) const {
		return memcmp(this, &info, sizeof(MaterialInfo)) == 0;
	}
};

class PoolRef : public Ref {
public:
	virtual ~PoolRef() {
		memory::pool::destroy(_pool);
	}
	PoolRef(memory::pool_t *root = nullptr) {
		_pool = memory::pool::create(root);
	}

	memory::pool_t *getPool() const { return _pool; }

	template <typename Callable>
	auto perform(const Callable &cb) {
		memory::pool::context<memory::pool_t *> ctx(_pool);
		return cb();
	}

protected:
	memory::pool_t *_pool = nullptr;
};

struct UpdateTime {
	// global OS timer at the start of update in microseconds
	uint64_t global;

	// microseconds since application was started
	uint64_t app;

	// microseconds since last update
	uint64_t delta;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_XLDEFINE_H_ */
