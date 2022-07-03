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

#ifndef XENOLITH_CORE_XLGRAPHICS_H_
#define XENOLITH_CORE_XLGRAPHICS_H_

#include "XLForward.h"
#include "XLGlEnum.h"

namespace stappler::xenolith {

using OpacityValue = ValueWrapper<uint8_t, class OpacityTag>;

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
	static const ColorMode SolidColor;
	static const ColorMode IntensityChannel;
	static const ColorMode AlphaChannel;

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
	: mode(Custom), r(stappler::toInt(r)), g(stappler::toInt(g)), b(stappler::toInt(b)), a(stappler::toInt(a)) { }

	ColorMode(gl::ComponentMapping color, gl::ComponentMapping a)
	: mode(Custom), r(stappler::toInt(color)), g(stappler::toInt(color)), b(stappler::toInt(color)), a(stappler::toInt(a)) { }

	ColorMode(const ColorMode &) = default;
	ColorMode(ColorMode &&) = default;
	ColorMode &operator=(const ColorMode &) = default;
	ColorMode &operator=(ColorMode &&) = default;

	bool operator==(const ColorMode &n) const = default;
	bool operator!=(const ColorMode &n) const = default;

	Mode getMode() const { return Mode(mode); }
	gl::ComponentMapping getR() const { return gl::ComponentMapping(r); }
	gl::ComponentMapping getG() const { return gl::ComponentMapping(g); }
	gl::ComponentMapping getB() const { return gl::ComponentMapping(b); }
	gl::ComponentMapping getA() const { return gl::ComponentMapping(a); }

	uint32_t toInt() const { return *(uint32_t *)this; }
	operator uint32_t () const { return *(uint32_t *)this; }
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

	bool operator==(const BlendInfo &other) const = default;
	bool operator!=(const BlendInfo &other) const = default;

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

	bool operator==(const DepthInfo &other) const = default;
	bool operator!=(const DepthInfo &other) const = default;
};

struct DepthBounds {
	bool enabled = false;
	float min = 0.0f;
	float max = 0.0f;

	bool operator==(const DepthBounds &other) const = default;
	bool operator!=(const DepthBounds &other) const = default;
};

struct StencilInfo {
	gl::StencilOp fail = gl::StencilOp::Keep;
	gl::StencilOp pass = gl::StencilOp::Keep;
	gl::StencilOp depthFail = gl::StencilOp::Keep;
	gl::CompareOp compare = gl::CompareOp::Never;
	uint32_t compareMask = 0;
	uint32_t writeMask = 0;
	uint32_t reference = 0;

	friend bool operator==(const StencilInfo &l, const StencilInfo &r) = default;
	friend bool operator!=(const StencilInfo &l, const StencilInfo &r) = default;
};

using LineWidth = ValueWrapper<float, class LineWidthFlag>;

class PipelineMaterialInfo {
public:
	PipelineMaterialInfo();
	PipelineMaterialInfo(const PipelineMaterialInfo &) = default;
	PipelineMaterialInfo &operator=(const PipelineMaterialInfo &) = default;

	template <typename T, typename ... Args, typename = std::enable_if_t<std::negation_v<std::is_same<T, PipelineMaterialInfo>>> >
	PipelineMaterialInfo(T &&t, Args && ... args) : PipelineMaterialInfo() {
		setup(std::forward<T>(t));
		setup(std::forward<Args>(args)...);
	}

	void setBlendInfo(const BlendInfo &);
	void setDepthInfo(const DepthInfo &);
	void setDepthBounds(const DepthBounds &);
	void enableStencil(const StencilInfo &);
	void enableStencil(const StencilInfo &front, const StencilInfo &back);
	void disableStancil();
	void setLineWidth(float lineWidth);

	const BlendInfo &getBlendInfo() const { return blend; }
	const DepthInfo &getDepthInfo() const { return depth; }
	const DepthBounds &getDepthBounds() const { return bounds; }

	bool isStancilEnabled() const { return stencil; }
	const StencilInfo &getStencilInfoFront() const { return front; }
	const StencilInfo &getStencilInfoBack() const { return back; }

	float getLineWidth() const { return lineWidth; }

	bool operator==(const PipelineMaterialInfo &tmp2) const {
		return this->blend == tmp2.blend && this->depth == tmp2.depth && this->bounds == tmp2.bounds
				&& this->stencil == tmp2.stencil && (!this->stencil || (this->front == tmp2.front && this->back == tmp2.back))
				&& this->lineWidth == tmp2.lineWidth;
	}

	bool operator!=(const PipelineMaterialInfo &tmp2) const {
		return this->blend != tmp2.blend || this->depth != tmp2.depth || this->bounds != tmp2.bounds
				|| this->stencil != tmp2.stencil || (this->stencil && (this->front != tmp2.front || this->back != tmp2.back))
				|| this->lineWidth != tmp2.lineWidth;
	}

	size_t hash() const {
		return hash::hashSize((const char *)this, sizeof(PipelineMaterialInfo));
	}

	String data() const;
	String description() const;

protected:
	void _setup(const BlendInfo &);
	void _setup(const DepthInfo &);
	void _setup(const DepthBounds &);
	void _setup(const StencilInfo &);
	void _setup(LineWidth lineWidth);

	template <typename T>
	void setup(T &&t) {
		_setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void setup(T &&t, Args && ... args) {
		setup(std::forward<T>(t));
		setup(std::forward<Args>(args)...);
	}

	void setup() { }

	BlendInfo blend;
	DepthInfo depth;
	DepthBounds bounds;
	StencilInfo front;
	StencilInfo back;
	uint32_t stencil = 0;
	float lineWidth = 0.0f; // 0.0f - draw triangles, < 0.0f - points,  > 0.0f - lines with width
};

enum class ColorMask : uint8_t {
	None = 0,
	R = 0x01,
	G = 0x02,
	B = 0x04,
	A = 0x08,
	Color = 0x07,
	All = 0x0F
};

SP_DEFINE_ENUM_AS_MASK(ColorMask)

static constexpr auto EmptyTextureName = "org.xenolith.EmptyImage";
static constexpr auto SolidTextureName = "org.xenolith.SolidImage";

struct MaterialInfo {
	std::array<uint64_t, config::MaxMaterialImages> images = { 0 };
	std::array<uint16_t, config::MaxMaterialImages> samplers = { 0 };
	std::array<ColorMode, config::MaxMaterialImages> colorModes = { ColorMode() };
	gl::MaterialType type = gl::MaterialType::Basic2D;
	PipelineMaterialInfo pipeline;

	uint64_t hash() const {
		return hash::hash64((const char *)this, sizeof(MaterialInfo));
	}

	String description() const;

	bool operator==(const MaterialInfo &info) const = default;
	bool operator!=(const MaterialInfo &info) const = default;
};

struct ZIndexLess {
	bool operator()(const SpanView<int16_t> &l, const SpanView<int16_t> &r) const noexcept {
		auto len = std::max(l.size(), r.size());
		for (size_t i = 0; i < len; ++ i) {
			auto valL = (i < l.size()) ? l.at(i) : 0;
			auto valR = (i < r.size()) ? r.at(i) : 0;

			if (valL != valR) {
				return valL < valR;
			}
		}
		return false;
	}
};

}

#endif /* XENOLITH_CORE_XLGRAPHICS_H_ */
