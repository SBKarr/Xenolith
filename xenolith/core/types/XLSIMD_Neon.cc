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

#include "simde/arm/neon.h"
#include "XLSIMD.h"
#include "XLVec4.h"

namespace stappler::xenolith {

static void multiplyVec4_Neon (const Vec4 &a, const Vec4 &b, Vec4 &dst) {
	simd::neon::multiplyVec4(&a.x, &b.x, &dst.x);
}

static void divideVec4_Neon (const Vec4 &a, const Vec4 &b, Vec4 &dst) {
	simd::neon::divideVec4(&a.x, &b.x, &dst.x);
}

static void addMat4Scalar_Neon(const Mat4 &m, float scalar, Mat4 &dst) {
	simd::neon::addMat4Scalar(m.m, scalar, dst.m);
}

static void addMat4_Neon(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::neon::addMat4(m1.m, m2.m, dst.m);
}

static void subtractMat4_Neon(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::neon::subtractMat4(m1.m, m2.m, dst.m);
}

static void multiplyMat4Scalar_Neon(const Mat4 &m, float scalar, Mat4 &dst) {
	simd::neon::multiplyMat4Scalar(m.m, scalar, dst.m);
}

static void multiplyMat4_Neon(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::neon::multiplyMat4(m1.m, m2.m, dst.m);
}

static void negateMat4_Neon(const Mat4 &m, Mat4 &dst) {
	simd::neon::negateMat4(m.m, dst.m);
}

static void transposeMat4_Neon(const Mat4 &m, Mat4 &dst) {
	simd::neon::transposeMat4(m.m, dst.m);
}

static void transformVec4Components_Neon (const Mat4 &m, float x, float y, float z, float w, Vec4 &dst) {
	simd::neon::transformVec4Components(m.m, x, y, z, w, &dst.x);
}

static void transformVec4_Neon(const Mat4 &m, const Vec4 &v, Vec4 &dst) {
	simd::neon::transformVec4(m.m, &v.x, &dst.x);
}

static void crossVec3_Neon(const Vec3 &v1, const Vec3 &v2, Vec3 &dst) {
	simd::neon::crossVec3(&v1.x, &v2.x, &dst.x);
}

[[maybe_unused]] static FunctionTable s_NeonFunctionTable = {
	multiplyVec4_Neon,
	divideVec4_Neon,
	addMat4Scalar_Neon,
	addMat4_Neon,
	subtractMat4_Neon,
	multiplyMat4Scalar_Neon,
	multiplyMat4_Neon,
	negateMat4_Neon,
	transposeMat4_Neon,
	transformVec4Components_Neon,
	transformVec4_Neon,
	crossVec3_Neon
};

}