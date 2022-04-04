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

#ifndef XENOLITH_CORE_TYPES_XLSIMD_H_
#define XENOLITH_CORE_TYPES_XLSIMD_H_

#include "SPCommon.h"

#define XL_DEFAULT_SIMD_SSE 1
#define XL_DEFAULT_SIMD_NEON 2
#define XL_DEFAULT_SIMD_NEON64 3

#if __SSE__
#define XL_DEFAULT_SIMD XL_DEFAULT_SIMD_SSE
#define XL_DEFAULT_SIMD_NAMESPACE sse
#elif __arm__
#define XL_DEFAULT_SIMD XL_DEFAULT_SIMD_NEON
#define XL_DEFAULT_SIMD_NAMESPACE neon
#elif __aarch64__
#define XL_DEFAULT_SIMD XL_DEFAULT_SIMD_NEON64
#define XL_DEFAULT_SIMD_NAMESPACE neon64
#else
#define XL_DEFAULT_SIMD XL_DEFAULT_SIMD_SSE
#define XL_DEFAULT_SIMD_NAMESPACE sse
#endif

#if XL_DEFAULT_SIMD == XL_DEFAULT_SIMD_NEON
#include "simde/arm/neon.h"
#include "simde/x86/sse.h"
#else
#include "simde/x86/sse.h"
#endif

#include "XLSIMD_Sse.h"
#include "XLSIMD_Neon.h"
#include "XLSIMD_Neon64.h"

namespace stappler::xenolith {

class Vec3;
class Vec4;
class Mat4;

struct FunctionTable {
	void (*multiplyVec4) (const Vec4 &, const Vec4 &, Vec4 &);
	void (*divideVec4) (const Vec4 &, const Vec4 &, Vec4 &);
	void (*addMat4Scalar) (const Mat4 &m, float scalar, Mat4 &dst);
	void (*addMat4) (const Mat4 &m1, const Mat4 &m2, Mat4 &dst);
	void (*subtractMat4) (const Mat4 &m1, const Mat4 &m2, Mat4 &dst);
	void (*multiplyMat4Scalar) (const Mat4 &m, float scalar, Mat4 &dst);
	void (*multiplyMat4) (const Mat4 &m1, const Mat4 &m2, Mat4 &dst);
	void (*negateMat4) (const Mat4 &m, Mat4 &dst);
	void (*transposeMat4) (const Mat4 &m, Mat4 &dst);
	void (*transformVec4Components) (const Mat4 &m, float x, float y, float z, float w, Vec4 &dst);
	void (*transformVec4) (const Mat4 &m, const Vec4 &, Vec4 &dst);
	void (*crossVec3) (const Vec3 &v1, const Vec3 &v2, Vec3 &dst);
};

extern FunctionTable *LayoutFunctionTable;

// in future, this will load more optimal LayoutFunctionTable based on cpuid
// instead of default compile-time selection (simde-based)
// noop for now
void initialize_simd();

}

namespace stappler::xenolith::simd {

inline void multiplyVec4(const float a[4], const float b[4], float dst[4]) {
	XL_DEFAULT_SIMD_NAMESPACE::multiplyVec4(a, b, dst);
}

inline void multiplyVec4Scalar(const float a[4], const float &b, float dst[4]) {
	XL_DEFAULT_SIMD_NAMESPACE::multiplyVec4Scalar(a, b, dst);
}

inline void divideVec4(const float a[4], const float b[4], float dst[4]) {
	XL_DEFAULT_SIMD_NAMESPACE::divideVec4(a, b, dst);
}

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::addMat4Scalar(m, scalar, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::addMat4(m1, m2, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::subtractMat4(m1, m2, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::multiplyMat4Scalar(m, scalar, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::multiplyMat4(m1, m2, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::negateMat4(m, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	XL_DEFAULT_SIMD_NAMESPACE::transposeMat4(m, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	XL_DEFAULT_SIMD_NAMESPACE::transformVec4Components(m, x, y, z, w, dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	XL_DEFAULT_SIMD_NAMESPACE::transformVec4(m, v, dst);
}

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	XL_DEFAULT_SIMD_NAMESPACE::crossVec3(v1, v2, dst);
}

}

#endif /* XENOLITH_CORE_TYPES_XLSIMD_H_ */
