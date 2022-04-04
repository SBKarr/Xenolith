/**
Copyright (c) 2016-2017 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_CORE_TYPES_XLPADDING_H_
#define XENOLITH_CORE_TYPES_XLPADDING_H_

#include "XLGeometry.h"
#include "XLVec2.h"

namespace stappler::xenolith {

struct Padding {
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	float left = 0.0f;

	constexpr float horizontal() const { return right + left; }
	constexpr float vertical() const { return top + bottom; }

	constexpr Vec2 getBottomLeft(const Size &size) const { return Vec2(left, bottom); }
	constexpr Vec2 getTopLeft(const Size &size) const { return Vec2(left, size.height - top); }

	constexpr Vec2 getBottomRight(const Size &size) const { return Vec2(size.width - right, bottom); }
	constexpr Vec2 getTopRight(const Size &size) const { return Vec2(size.width - right, size.height - top); }

	constexpr Padding &setTop(float value) { top = value; return *this; }
	constexpr Padding &setBottom(float value) { bottom = value; return *this; }
	constexpr Padding &setLeft(float value) { left = value; return *this; }
	constexpr Padding &setRight(float value) { right = value; return *this; }

	constexpr Padding &set(float vtop, float vright, float vbottom, float vleft) {
		top = vtop; right = vright; bottom = vbottom; left = vleft; return *this;
	}

	constexpr Padding &set(float vtop, float rightAndLeft, float vbottom) {
		top = vtop; right = rightAndLeft; bottom = vbottom; left = rightAndLeft; return *this;
	}

	constexpr Padding &set(float topAndBottom, float rightAndLeft) {
		top = topAndBottom; right = rightAndLeft; bottom = topAndBottom; left = rightAndLeft; return *this;
	}

	constexpr Padding &set(float all) {
		top = all; right = all; bottom = all; left = all; return *this;
	}

	constexpr Padding(float vtop, float vright, float vbottom, float vleft) {
		top = vtop; right = vright; bottom = vbottom; left = vleft;
	}

	constexpr Padding(float vtop, float rightAndLeft, float vbottom) {
		top = vtop; right = rightAndLeft; bottom = vbottom; left = rightAndLeft;
	}

	constexpr Padding(float topAndBottom, float rightAndLeft) {
		top = topAndBottom; right = rightAndLeft; bottom = topAndBottom; left = rightAndLeft;
	}

	constexpr Padding(float all) {
		top = all; right = all; bottom = all; left = all;
	}

	constexpr Padding() = default;
	constexpr Padding(const Padding &) = default;

	constexpr bool operator == (const Padding &p) const {
		return std::fabs(top - p.top) < std::numeric_limits<float>::epsilon()
				&& std::fabs(bottom - p.bottom) < std::numeric_limits<float>::epsilon()
				&& std::fabs(left - p.left) < std::numeric_limits<float>::epsilon()
				&& std::fabs(right - p.right) < std::numeric_limits<float>::epsilon();
	}

	constexpr bool operator != (const Padding &p) const {
		return !(*this == p);
	}

	constexpr Padding & operator *= (const float &v) { top *= v; right *= v; bottom *= v; left *= v; return *this; }
};

using Margin = Padding;

}

#endif /* XENOLITH_CORE_TYPES_XLPADDING_H_ */
