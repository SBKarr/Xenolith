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

#include "simde/x86/sse.h"
#include "XLSIMD.h"
#include "XLVec4.h"
#include "XLMat4.h"

namespace stappler::xenolith {

static void multiplyVec4_Sse (const Vec4 &a, const Vec4 &b, Vec4 &dst) {
	simd::sse::multiplyVec4(&a.x, &b.x, &dst.x);
}

static void divideVec4_Sse (const Vec4 &a, const Vec4 &b, Vec4 &dst) {
	simd::sse::divideVec4(&a.x, &b.x, &dst.x);
}

static void addMat4Scalar_Sse(const Mat4 &m, float scalar, Mat4 &dst) {
	simd::sse::addMat4Scalar(m.m, scalar, dst.m);
}

static void addMat4_Sse(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::sse::addMat4(m1.m, m2.m, dst.m);
}

static void subtractMat4_Sse(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::sse::subtractMat4(m1.m, m2.m, dst.m);
}

static void multiplyMat4Scalar_Sse(const Mat4 &m, float scalar, Mat4 &dst) {
	simd::sse::multiplyMat4Scalar(m.m, scalar, dst.m);
}

static void multiplyMat4_Sse(const Mat4 &m1, const Mat4 &m2, Mat4 &dst) {
	simd::sse::multiplyMat4(m1.m, m2.m, dst.m);
}

static void negateMat4_Sse(const Mat4 &m, Mat4 &dst) {
	simd::sse::negateMat4(m.m, dst.m);
}

static void transposeMat4_Sse(const Mat4 &m, Mat4 &dst) {
	simd::sse::transposeMat4(m.m, dst.m);
}

static void transformVec4Components_Sse (const Mat4 &m, float x, float y, float z, float w, Vec4 &dst) {
	simd::sse::transformVec4Components(m.m, x, y, z, w, &dst.x);
}

static void transformVec4_Sse(const Mat4 &m, const Vec4 &v, Vec4 &dst) {
	simd::sse::transformVec4(m.m, &v.x, &dst.x);
}

static void crossVec3_Sse(const Vec3 &v1, const Vec3 &v2, Vec3 &dst) {
	simd::sse::crossVec3(&v1.x, &v2.x, &dst.x);
}

[[maybe_unused]] static FunctionTable s_SseFunctionTable = {
	multiplyVec4_Sse,
	divideVec4_Sse,
	addMat4Scalar_Sse,
	addMat4_Sse,
	subtractMat4_Sse,
	multiplyMat4Scalar_Sse,
	multiplyMat4_Sse,
	negateMat4_Sse,
	transposeMat4_Sse,
	transformVec4Components_Sse,
	transformVec4_Sse,
	crossVec3_Sse
};

}
