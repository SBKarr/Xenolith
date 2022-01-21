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

SP_DEFINE_ENUM_AS_MASK(NodeFlags)

/**
 * ColorMode defines how to map texture color to shader color representation
 *
 * In Solid mode, texture color will be sent directly to shader
 * In Custom mode, you can define individual mapping for each channel
 *
 * (see gl::ComponentMapping)
 */
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

	ColorMode(gl::ComponentMapping color, gl::ComponentMapping a)
	: mode(Custom), r(toInt(color)), g(toInt(color)), b(toInt(color)), a(toInt(a)) { }

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

// uint32_t-sized blend description
struct BlendInfo {
	uint32_t enabled : 4;
	uint32_t srcColor : 4;
	uint32_t dstColor : 4;
	uint32_t opColor : 4;
	uint32_t srcAlpha : 4;
	uint32_t dstAlpha : 4;
	uint32_t opAlpha : 4;
	uint32_t writeMask : 4;

	BlendInfo() : enabled(0)
	, srcColor(toInt(gl::BlendFactor::One)) , dstColor(toInt(gl::BlendFactor::OneMinusSrcAlpha)) , opColor(toInt(gl::BlendOp::Add))
	, srcAlpha(toInt(gl::BlendFactor::One)) , dstAlpha(toInt(gl::BlendFactor::OneMinusSrcAlpha)) , opAlpha(toInt(gl::BlendOp::Add))
	, writeMask(toInt(gl::ColorComponentFlags::All)) { }

	BlendInfo(gl::BlendFactor src, gl::BlendFactor dst, gl::BlendOp op = gl::BlendOp::Add,
			gl::ColorComponentFlags flags = gl::ColorComponentFlags::All)
	: enabled(1), srcColor(toInt(src)), dstColor(toInt(dst)), opColor(toInt(op))
	, srcAlpha(toInt(src)), dstAlpha(toInt(dst)), opAlpha(toInt(op)), writeMask(toInt(flags)) { }

	BlendInfo(gl::BlendFactor srcColor, gl::BlendFactor dstColor, gl::BlendOp opColor,
			gl::BlendFactor srcAlpha, gl::BlendFactor dstAlpha, gl::BlendOp opAlpha,
			gl::ColorComponentFlags flags = gl::ColorComponentFlags::All)
	: enabled(1), srcColor(toInt(srcColor)), dstColor(toInt(dstColor)), opColor(toInt(opColor))
	, srcAlpha(toInt(srcAlpha)), dstAlpha(toInt(dstAlpha)), opAlpha(toInt(opAlpha)), writeMask(toInt(flags)) { }

	bool operator==(const BlendInfo &other) const {
		return memcmp(this, &other, sizeof(BlendInfo)) == 0;
	}

	bool operator!=(const BlendInfo &other) const {
		return memcmp(this, &other, sizeof(BlendInfo)) != 0;
	}

	bool isEnabled() const {
		return enabled != 0;
	}
};

struct DepthInfo {
	uint32_t writeEnabled : 4;
	uint32_t testEnabled : 4;
	uint32_t compare : 24; // gl::CompareOp

	DepthInfo() : writeEnabled(0), testEnabled(0), compare(0) { }

	DepthInfo(bool write, bool test, gl::CompareOp compareOp)
	: writeEnabled(write ? 1 : 0), testEnabled(test ? 1 : 0), compare(toInt(compareOp)) { }

	bool operator==(const BlendInfo &other) const {
		return memcmp(this, &other, sizeof(BlendInfo)) == 0;
	}

	bool operator!=(const BlendInfo &other) const {
		return memcmp(this, &other, sizeof(BlendInfo)) != 0;
	}
};

struct DepthBounds {
	bool enabled = false;
	float min = 0.0f;
	float max = 0.0f;
};

struct StencilInfo {
	gl::StencilOp fail = gl::StencilOp::Keep;
	gl::StencilOp pass = gl::StencilOp::Keep;
	gl::StencilOp depthFail = gl::StencilOp::Keep;
	gl::CompareOp compare = gl::CompareOp::Never;
	uint32_t compareMask = 0;
	uint32_t writeMask = 0;
	uint32_t reference = 0;
};

struct PipelineMaterialInfo {
	BlendInfo blend;
	DepthInfo depth;
	DepthBounds bounds;
	bool stencil = false;
	StencilInfo front;
	StencilInfo back;

	size_t hash() const {
		auto tmp = *this; tmp.normalize();
		return hash::hashSize((const char *)&tmp, sizeof(PipelineMaterialInfo));
	}

	bool operator==(const PipelineMaterialInfo &other) const {
		auto tmp1 = *this; tmp1.normalize();
		auto tmp2 = other; tmp2.normalize();
		return memcmp(&tmp1, &tmp2, sizeof(PipelineMaterialInfo)) == 0;
	}

	bool operator!=(const PipelineMaterialInfo &other) const {
		auto tmp1 = *this; tmp1.normalize();
		auto tmp2 = other; tmp2.normalize();
		return memcmp(&tmp1, &tmp2, sizeof(PipelineMaterialInfo)) != 0;
	}

	String data() const {
		auto tmp = *this; tmp.normalize();
		return base16::encode(BytesView((const uint8_t *)&tmp, sizeof(PipelineMaterialInfo)));
	}

	void normalize() {
		if (!blend.isEnabled()) {
			blend = BlendInfo();
		}
		if (!depth.testEnabled) {
			depth.compare = 0;
		} else {
			depth.testEnabled = 1;
		}
		if (depth.writeEnabled) {
			depth.writeEnabled = 1;
		}
		if (!bounds.enabled) {
			bounds.min = 0.0f;
			bounds.max = 0.0f;
		}
		if (!stencil) {
			front = StencilInfo();
			back = StencilInfo();
		}
	}
};

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
	PipelineMaterialInfo pipeline;

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
