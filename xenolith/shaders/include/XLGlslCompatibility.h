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

#ifndef XENOLITH_SHADERS_INCLUDE_XLGLSLCOMPATIBILITY_H_
#define XENOLITH_SHADERS_INCLUDE_XLGLSLCOMPATIBILITY_H_

#if XL_GLSL

#define color4 vec4
#define XL_GLSL_IN in
#define XL_GLSL_INLINE

#else

#include "XLGl.h"

#define XL_GLSL_IN
#define XL_GLSL_INLINE inline

namespace stappler::xenolith::gl::glsl {

using vec2 = Vec2;
using vec3 = Vec3;
using vec4 = Vec4;
using mat4 = Mat4;
using uint = uint32_t;
using color4 = Color4F;

template <typename T>
inline float dot(const T &v1, const T &v2) {
	return T::dot(v1, v2);
}

template <typename T>
inline T cross(const T &v1, const T &v2) {
	T ret;
	T::cross(v1, v2, &ret);
	return ret;
}

using math::clamp;

template <typename T>
inline T sign(const T &value) {
	return std::copysign(T(1), value);
}

template <typename T>
inline float length(const T &value) {
	return value.length();
}

template <typename T>
inline T abs(const T &value) {
	return value.getAbs();
}

template <typename T>
inline auto lessThanEqual(const T &v1, const T &v2) {
	return geom::lessThanEqual(v1, v2);
}

template <typename T>
inline bool all(const T &v) {
	return v.all();
}

template <typename T>
inline bool any(const T &v) {
	return v.any();
}

template <typename T>
inline bool none(const T &v) {
	return v.none();
}

}

#endif

#endif /* XENOLITH_SHADERS_INCLUDE_XLGLSLCOMPATIBILITY_H_ */
