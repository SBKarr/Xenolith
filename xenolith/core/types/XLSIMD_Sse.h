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


#ifndef XENOLITH_CORE_TYPES_XLSIMD_SSE_H_
#define XENOLITH_CORE_TYPES_XLSIMD_SSE_H_

#include "simde/x86/sse.h"

#if __SSE__
#define XL_SSE_STORE_VEC4(vec, value)	*((simde__m128 *)&vec.x) = (value)
#define XL_SSE_LOAD_VEC4(vec)			*((simde__m128 *)(&vec.x))
#else
#define XL_SSE_STORE_VEC4(vec, value)	simde_mm_store_ps(&vec.x, value)
#define XL_SSE_LOAD_VEC4(vec)			simde_mm_load_ps(&vec.x)
#endif

namespace stappler::xenolith::simd::sse {

inline void loadMat4_impl(const float m[16], simde__m128 dst[4]) [[maybe_unused]] {
	dst[0] = simde_mm_load_ps(&m[0]);
	dst[1] = simde_mm_load_ps(&m[4]);
	dst[2] = simde_mm_load_ps(&m[8]);
	dst[3] = simde_mm_load_ps(&m[12]);
}

inline void storeMat4_impl(const simde__m128 m[4], float dst[16]) [[maybe_unused]] {
	simde_mm_store_ps((simde_float32 *)&dst[0], m[0]);
	simde_mm_store_ps((simde_float32 *)&dst[4], m[1]);
	simde_mm_store_ps((simde_float32 *)&dst[8], m[2]);
	simde_mm_store_ps((simde_float32 *)&dst[12], m[3]);
}

inline void addMat4Scalar_impl(const simde__m128 *m, float scalar, simde__m128 *dst) {
	auto s = simde_mm_set1_ps(scalar);
	dst[0] = simde_mm_add_ps(m[0], s);
	dst[1] = simde_mm_add_ps(m[1], s);
	dst[2] = simde_mm_add_ps(m[2], s);
	dst[3] = simde_mm_add_ps(m[3], s);
}

inline void addMat4_impl(const simde__m128 *m1, const simde__m128 *m2, simde__m128 *dst) {
	dst[0] = simde_mm_add_ps(m1[0], m2[0]);
	dst[1] = simde_mm_add_ps(m1[1], m2[1]);
	dst[2] = simde_mm_add_ps(m1[2], m2[2]);
	dst[3] = simde_mm_add_ps(m1[3], m2[3]);
}

inline void subtractMat4_impl(const simde__m128 *m1, const simde__m128 *m2, simde__m128 *dst) {
	dst[0] = simde_mm_sub_ps(m1[0], m2[0]);
	dst[1] = simde_mm_sub_ps(m1[1], m2[1]);
	dst[2] = simde_mm_sub_ps(m1[2], m2[2]);
	dst[3] = simde_mm_sub_ps(m1[3], m2[3]);
}

inline void multiplyMat4Scalar_impl(const simde__m128 *m, float scalar, simde__m128 *dst) {
	auto s = simde_mm_set1_ps(scalar);
	dst[0] = simde_mm_mul_ps(m[0], s);
	dst[1] = simde_mm_mul_ps(m[1], s);
	dst[2] = simde_mm_mul_ps(m[2], s);
	dst[3] = simde_mm_mul_ps(m[3], s);
}

inline void multiplyMat4_impl(const simde__m128 m1[4], const simde__m128 m2[4], simde__m128 dst[4]) {
	simde__m128 dst0, dst1, dst2, dst3;
	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst0 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst1 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst2 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst3 = a2;
	}
	dst[0] = dst0;
	dst[1] = dst1;
	dst[2] = dst2;
	dst[3] = dst3;
}

inline void negateMat4_impl(const simde__m128 m[4], simde__m128 dst[4]) {
	simde__m128 z = simde_mm_setzero_ps();
	dst[0] = simde_mm_sub_ps(z, m[0]);
	dst[1] = simde_mm_sub_ps(z, m[1]);
	dst[2] = simde_mm_sub_ps(z, m[2]);
	dst[3] = simde_mm_sub_ps(z, m[3]);
}

inline void transposeMat4_impl(const simde__m128 m[4], simde__m128 dst[4]) {
	simde__m128 tmp0 = simde_mm_shuffle_ps(m[0], m[1], 0x44);
	simde__m128 tmp2 = simde_mm_shuffle_ps(m[0], m[1], 0xEE);
	simde__m128 tmp1 = simde_mm_shuffle_ps(m[2], m[3], 0x44);
	simde__m128 tmp3 = simde_mm_shuffle_ps(m[2], m[3], 0xEE);

	dst[0] = simde_mm_shuffle_ps(tmp0, tmp1, 0x88);
	dst[1] = simde_mm_shuffle_ps(tmp0, tmp1, 0xDD);
	dst[2] = simde_mm_shuffle_ps(tmp2, tmp3, 0x88);
	dst[3] = simde_mm_shuffle_ps(tmp2, tmp3, 0xDD);
}

inline void transformVec4Components_impl(const simde__m128 m[4], float x, float y, float z, float w, simde__m128& dst) {
	simde__m128 col1 = simde_mm_set1_ps(x);
	simde__m128 col2 = simde_mm_set1_ps(y);
	simde__m128 col3 = simde_mm_set1_ps(z);
	simde__m128 col4 = simde_mm_set1_ps(w);

	dst = simde_mm_add_ps(
			simde_mm_add_ps(simde_mm_mul_ps(m[0], col1), simde_mm_mul_ps(m[1], col2)),
			simde_mm_add_ps(simde_mm_mul_ps(m[2], col3), simde_mm_mul_ps(m[3], col4))
	);
}

inline void transformVec4_impl(const simde__m128 m[4], const simde__m128 &v, simde__m128& dst) {
	simde__m128 col1 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(0, 0, 0, 0));
	simde__m128 col2 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(1, 1, 1, 1));
	simde__m128 col3 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(2, 2, 2, 2));
	simde__m128 col4 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(3, 3, 3, 3));

	dst = simde_mm_add_ps(
		simde_mm_add_ps(simde_mm_mul_ps(m[0], col1), simde_mm_mul_ps(m[1], col2)),
		simde_mm_add_ps(simde_mm_mul_ps(m[2], col3), simde_mm_mul_ps(m[3], col4))
	);
}

#if XL_DEFAULT_SIMD == XL_DEFAULT_SIMD_SSE

inline void multiplyVec4(const float a[4], const float b[4], float dst[4]) {
	*((simde__m128 *)dst) = simde_mm_mul_ps( *(simde__m128 *)a, *(simde__m128 *)b);
}

inline void multiplyVec4Scalar(const float a[4], const float &b, float dst[4]) {
	*((simde__m128 *)dst) = simde_mm_mul_ps( *(simde__m128 *)a, simde_mm_load1_ps(&b));
}

inline void divideVec4(const float a[4], const float b[4], float dst[4]) {
	*((simde__m128 *)dst) = simde_mm_div_ps( *(simde__m128 *)a, *(simde__m128 *)b);
}

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	addMat4Scalar_impl((const simde__m128 *)m, scalar, (simde__m128 *)dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	addMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	subtractMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	multiplyMat4Scalar_impl((const simde__m128 *)m, scalar, (simde__m128 *)dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	multiplyMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	negateMat4_impl((const simde__m128 *)m, (simde__m128 *)dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	transposeMat4_impl((const simde__m128 *)m, (simde__m128 *)dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	transformVec4Components_impl((const simde__m128 *)m, x, y, z, w, *(simde__m128 *)dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	transformVec4_impl((const simde__m128 *)m, *(const simde__m128 *)v, *(simde__m128 *)dst);
}

#else

inline void multiplyVec4(const float a[4], const float b[4], float dst[4]) {
	simde_mm_store_ps(dst,
		simde_mm_mul_ps(
			simde_mm_load_ps(a),
			simde_mm_load_ps(b)));
}

inline void multiplyVec4Scalar(const float a[4], const float &b, float dst[4]) {
	simde_mm_store_ps(dst,
		simde_mm_mul_ps(
			simde_mm_load_ps(a),
			simde_mm_load_ps1(&b)));
}

inline void divideVec4(const float a[4], const float b[4], float dst[4]) {
	simde_mm_store_ps(dst,
		simde_mm_div_ps(
			simde_mm_load_ps(a),
			simde_mm_load_ps(b)));
}

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	addMat4Scalar_impl(tmpM, scalar, dstM);
	storeMat4_impl(dstM, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	addMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	subtractMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	multiplyMat4Scalar_impl(tmpM, scalar, dstM);
	storeMat4_impl(dstM, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	multiplyMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	negateMat4_impl(tmpM, dstM);
	storeMat4_impl(dstM, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	transposeMat4_impl(tmpM, dstM);
	storeMat4_impl(dstM, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	simde__m128 tmpM[4];
	simde__m128 dstV;
	loadMat4_impl(m, tmpM);

	transformVec4Components_impl(tmpM, x, y, z, w, dstV);
	simde_mm_store_ps((simde_float32 *)dst, dstV);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	simde__m128 tmpM[4];
	simde__m128 dstV;
	loadMat4_impl(m, tmpM);

	transformVec4_impl(tmpM, simde_mm_load_ps(v), dstV);
	simde_mm_store_ps((simde_float32 *)&dst.x, dstV);
}

#endif

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	const float x = (v1[1] * v2[2]) - (v1[2] * v2[1]);
	const float y = (v1[2] * v2[0]) - (v1[0] * v2[2]);
	const float z = (v1[0] * v2[1]) - (v1[1] * v2[0]);

	dst[0] = x;
	dst[1] = y;
	dst[2] = z;
}

}

#endif /* XENOLITH_CORE_TYPES_XLSIMD_SSE_H_ */
