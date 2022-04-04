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


#ifndef XENOLITH_CORE_TYPES_XLSIMD_NEON64_H_
#define XENOLITH_CORE_TYPES_XLSIMD_NEON64_H_

#include "simde/arm/neon.h"

namespace stappler::xenolith::simd::neon64 {

#if XL_DEFAULT_SIMD == XL_DEFAULT_SIMD_NEON64

static inline void addMat4Scalar_Neon64_impl(const float* m, float scalar, float* dst) {
	asm volatile (
		"ld4  {v0.4s, v1.4s, v2.4s, v3.4s}, [%1]    	\n\t" // M[m0-m7] M[m8-m15]
		"ld1r {v4.4s}, [%2]				                \n\t"//ssss

		"fadd v8.4s, v0.4s, v4.4s			\n\t"// DST->M[m0-m3] = M[m0-m3] + s
		"fadd v9.4s, v1.4s, v4.4s			\n\t"// DST->M[m4-m7] = M[m4-m7] + s
		"fadd v10.4s, v2.4s, v4.4s			\n\t"// DST->M[m8-m11] = M[m8-m11] + s
		"fadd v11.4s, v3.4s, v4.4s			\n\t"// DST->M[m12-m15] = M[m12-m15] + s

		"st4 {v8.4s, v9.4s, v10.4s, v11.4s}, [%0] 	\n\t"// Result in V9
		:
		: "r"(dst), "r"(m), "r"(&scalar)
		: "v0", "v1", "v2", "v3", "v4", "v8", "v9", "v10", "v11", "memory"
	);
}

static inline void addMat4_Neon64_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"ld4     {v0.4s, v1.4s, v2.4s, v3.4s},     [%1] 	\n\t" // M1[m0-m7] M1[m8-m15]
			"ld4     {v8.4s, v9.4s, v10.4s, v11.4s},   [%2] 	\n\t"// M2[m0-m7] M2[m8-m15]

			"fadd   v12.4s, v0.4s, v8.4s          \n\t"// DST->M[m0-m3] = M1[m0-m3] + M2[m0-m3]
			"fadd   v13.4s, v1.4s, v9.4s          \n\t"// DST->M[m4-m7] = M1[m4-m7] + M2[m4-m7]
			"fadd   v14.4s, v2.4s, v10.4s         \n\t"// DST->M[m8-m11] = M1[m8-m11] + M2[m8-m11]
			"fadd   v15.4s, v3.4s, v11.4s         \n\t"// DST->M[m12-m15] = M1[m12-m15] + M2[m12-m15]

			"st4    {v12.4s, v13.4s, v14.4s, v15.4s}, [%0]    \n\t"// DST->M[m0-m7] DST->M[m8-m15]
			:
			: "r"(dst), "r"(m1), "r"(m2)
			: "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory"
	);
}

static inline void subtractMat4_Neon64_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"ld4     {v0.4s, v1.4s, v2.4s, v3.4s},     [%1]  \n\t" // M1[m0-m7] M1[m8-m15]
			"ld4     {v8.4s, v9.4s, v10.4s, v11.4s},   [%2]  \n\t"// M2[m0-m7] M2[m8-m15]

			"fsub   v12.4s, v0.4s, v8.4s         \n\t"// DST->M[m0-m3] = M1[m0-m3] - M2[m0-m3]
			"fsub   v13.4s, v1.4s, v9.4s         \n\t"// DST->M[m4-m7] = M1[m4-m7] - M2[m4-m7]
			"fsub   v14.4s, v2.4s, v10.4s        \n\t"// DST->M[m8-m11] = M1[m8-m11] - M2[m8-m11]
			"fsub   v15.4s, v3.4s, v11.4s        \n\t"// DST->M[m12-m15] = M1[m12-m15] - M2[m12-m15]

			"st4    {v12.4s, v13.4s, v14.4s, v15.4s}, [%0]   \n\t"// DST->M[m0-m7] DST->M[m8-m15]
			:
			: "r"(dst), "r"(m1), "r"(m2)
			: "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "memory"
	);
}

static inline void multiplyMat4Scalar_Neon64_impl(const float* m, float scalar, float* dst) {
	asm volatile (
			"ld1     {v0.s}[0],         [%2]            \n\t" //s
			"ld4     {v4.4s, v5.4s, v6.4s, v7.4s}, [%1]       \n\t"//M[m0-m7] M[m8-m15]

			"fmul     v8.4s, v4.4s, v0.s[0]               \n\t"// DST->M[m0-m3] = M[m0-m3] * s
			"fmul     v9.4s, v5.4s, v0.s[0]               \n\t"// DST->M[m4-m7] = M[m4-m7] * s
			"fmul     v10.4s, v6.4s, v0.s[0]              \n\t"// DST->M[m8-m11] = M[m8-m11] * s
			"fmul     v11.4s, v7.4s, v0.s[0]              \n\t"// DST->M[m12-m15] = M[m12-m15] * s

			"st4     {v8.4s, v9.4s, v10.4s, v11.4s},           [%0]     \n\t"// DST->M[m0-m7] DST->M[m8-m15]
			:
			: "r"(dst), "r"(m), "r"(&scalar)
			: "v0", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "memory"
	);
}

static inline void multiplyMat4_Neon64_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"ld1     {v8.4s, v9.4s, v10.4s, v11.4s}, [%1] \n\t"       // M1[m0-m7] M1[m8-m15] M2[m0-m7]  M2[m8-m15]
			"ld4     {v0.4s, v1.4s, v2.4s, v3.4s},  [%2]   \n\t"// M2[m0-m15]

			"fmul    v12.4s, v8.4s, v0.s[0]     \n\t"// DST->M[m0-m3] = M1[m0-m3] * M2[m0]
			"fmul    v13.4s, v8.4s, v0.s[1]     \n\t"// DST->M[m4-m7] = M1[m4-m7] * M2[m4]
			"fmul    v14.4s, v8.4s, v0.s[2]     \n\t"// DST->M[m8-m11] = M1[m8-m11] * M2[m8]
			"fmul    v15.4s, v8.4s, v0.s[3]     \n\t"// DST->M[m12-m15] = M1[m12-m15] * M2[m12]

			"fmla    v12.4s, v9.4s, v1.s[0]     \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m1]
			"fmla    v13.4s, v9.4s, v1.s[1]     \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m5]
			"fmla    v14.4s, v9.4s, v1.s[2]     \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m9]
			"fmla    v15.4s, v9.4s, v1.s[3]     \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m13]

			"fmla    v12.4s, v10.4s, v2.s[0]    \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m2]
			"fmla    v13.4s, v10.4s, v2.s[1]    \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m6]
			"fmla    v14.4s, v10.4s, v2.s[2]    \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m10]
			"fmla    v15.4s, v10.4s, v2.s[3]    \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m14]

			"fmla    v12.4s, v11.4s, v3.s[0]    \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m3]
			"fmla    v13.4s, v11.4s, v3.s[1]    \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m7]
			"fmla    v14.4s, v11.4s, v3.s[2]    \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m11]
			"fmla    v15.4s, v11.4s, v3.s[3]    \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m15]

			"st1    {v12.4s, v13.4s, v14.4s, v15.4s}, [%0]  \n\t"// DST->M[m0-m7]// DST->M[m8-m15]

			:// output
			: "r"(dst), "r"(m1), "r"(m2)// input - note *value* of pointer doesn't change.
			: "memory", "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15"
	);
}

static inline void negateMat4_Neon64_impl(const float* m, float* dst) {
	asm volatile (
			"ld4     {v0.4s, v1.4s, v2.4s, v3.4s},  [%1]     \n\t" // load m0-m7 load m8-m15

			"fneg     v4.4s, v0.4s             \n\t"// negate m0-m3
			"fneg     v5.4s, v1.4s             \n\t"// negate m4-m7
			"fneg     v6.4s, v2.4s             \n\t"// negate m8-m15
			"fneg     v7.4s, v3.4s             \n\t"// negate m8-m15

			"st4     {v4.4s, v5.4s, v6.4s, v7.4s},  [%0]     \n\t"// store m0-m7 store m8-m15
			:
			: "r"(dst), "r"(m)
			: "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory"
	);
}

static inline void transposeMat4_Neon64_impl(const float* m, float* dst) {
	asm volatile (
			"ld4 {v0.4s, v1.4s, v2.4s, v3.4s}, [%1]    \n\t" // DST->M[m0, m4, m8, m12] = M[m0-m3]
			//DST->M[m1, m5, m9, m12] = M[m4-m7]
			"st1 {v0.4s, v1.4s, v2.4s, v3.4s}, [%0]    \n\t"
			:
			: "r"(dst), "r"(m)
			: "v0", "v1", "v2", "v3", "memory"
	);
}

static inline void transformVec4Components_Neon64_impl(const float* m, float x, float y, float z, float w, float* dst) {
	asm volatile (
			"ld1    {v0.s}[0],        [%1]    \n\t"    // V[x]
			"ld1    {v0.s}[1],        [%2]    \n\t"// V[y]
			"ld1    {v0.s}[2],        [%3]    \n\t"// V[z]
			"ld1    {v0.s}[3],        [%4]    \n\t"// V[w]
			"ld1    {v9.4s, v10.4s, v11.4s, v12.4s}, [%5]   \n\t"// M[m0-m7] M[m8-m15]

			"fmul v13.4s, v9.4s, v0.s[0]           \n\t"// DST->V = M[m0-m3] * V[x]
			"fmla v13.4s, v10.4s, v0.s[1]           \n\t"// DST->V += M[m4-m7] * V[y]
			"fmla v13.4s, v11.4s, v0.s[2]           \n\t"// DST->V += M[m8-m11] * V[z]
			"fmla v13.4s, v12.4s, v0.s[3]           \n\t"// DST->V += M[m12-m15] * V[w]

			//"st1 {v13.4s}, [%0]               \n\t"    // DST->V[x, y] // DST->V[z]
			"st1 {v13.2s}, [%0], 8               \n\t"
			"st1 {v13.s}[2], [%0]                \n\t"
			:
			: "r"(dst), "r"(&x), "r"(&y), "r"(&z), "r"(&w), "r"(m)
			: "v0", "v9", "v10","v11", "v12", "v13", "memory"
	);
}

static inline void transformVec4_Neon64_impl(const float* m, const float* v, float* dst) {
	asm volatile (
			"ld1    {v0.4s}, [%1]     \n\t"   // V[x, y, z, w]
			"ld1    {v9.4s, v10.4s, v11.4s, v12.4s}, [%2] \n\t"// M[m0-m7] M[m8-m15]

			"fmul   v13.4s, v9.4s, v0.s[0]     \n\t"// DST->V = M[m0-m3] * V[x]
			"fmla   v13.4s, v10.4s, v0.s[1]    \n\t"// DST->V = M[m4-m7] * V[y]
			"fmla   v13.4s, v11.4s, v0.s[2]    \n\t"// DST->V = M[m8-m11] * V[z]
			"fmla   v13.4s, v12.4s, v0.s[3]    \n\t"// DST->V = M[m12-m15] * V[w]

			"st1    {v13.4s}, [%0]  	 \n\t"// DST->V
			:
			: "r"(dst), "r"(v), "r"(m)
			: "v0", "v9", "v10","v11", "v12", "v13", "memory"
	);
}

static inline void crossVec3_Neon64_impl(const float* v1, const float* v2, float* dst) {
	asm volatile (
			"ld1 {v0.2s},  [%2]           \n\t"
			"ld1 {v0.s}[2],  [%1]           \n\t"
			"mov v0.s[3], v0.s[0]         \n\t" // q0 = (v1y, v1z, v1x, v1x)

			"ld1 {v1.4s},  [%3]           \n\t"
			"mov v1.s[3], v1.s[0]           \n\t"// q1 = (v2x, v2y, v2z, v2x)

			"fmul v2.4s, v0.4s, v1.4s            \n\t"// x = v1y * v2z, y = v1z * v2x

			"mov v0.s[0], v0.s[1]           \n\t"
			"mov v0.s[1], v0.s[2]           \n\t"
			"mov v0.s[2], v0.s[3]           \n\t"

			"mov v1.s[3], v1.s[2]           \n\t"

			"fmul v0.4s, v0.4s, v1.4s            \n\t"

			"mov v0.s[3], v0.s[1]           \n\t"
			"mov v0.s[1], v0.s[2]           \n\t"
			"mov v0.s[2], v0.s[0]           \n\t"

			"fsub v2.4s, v0.4s, v2.4s            \n\t"

			"mov v2.s[0], v2.s[1]           \n\t"
			"mov v2.s[1], v2.s[2]           \n\t"
			"mov v2.s[2], v2.s[3]           \n\t"

			"st1 {v2.2s},       [%0], 8      \n\t"// V[x, y]
			"st1 {v2.s}[2],     [%0]         \n\t"// V[z]
			:
			: "r"(dst), "r"(v1), "r"((v1+1)), "r"(v2), "r"((v2+1))
			: "v0", "v1", "v2", "memory"
	);
}

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	addMat4Scalar_impl(m, scalar, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	addMat4_impl(m1, m2, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	subtractMat4_impl(m1, m2, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	multiplyMat4Scalar_impl(m, scalar, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	multiplyMat4_impl(m1, m2, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	negateMat4_impl(m, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	transposeMat4_impl(m, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	transformVec4Components_impl(m, x, y, z, w, dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	transformVec4_impl(m, v, dst);
}

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	crossVec3_impl(v1, v2, dst);
}

#else

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	sse::addMat4Scalar(m, scalar, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::addMat4(m1, m2, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::subtractMat4(m1, m2, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	sse::multiplyMat4Scalar(m, scalar, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::multiplyMat4(m1, m2, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	sse::negateMat4(m, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	sse::transposeMat4(m, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	sse::transformVec4Components(m, x, y, z, w, dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	sse::transformVec4(m, v, dst);
}

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	sse::crossVec3(v1, v2, dst);
}

#endif

inline void multiplyVec4(const float a[4], const float b[4], float dst[4]) {
	simde_vst1q_f32(dst,
		simde_vmulq_f32(
			simde_vld1q_f32(a),
			simde_vld1q_f32(b)));
}

inline void multiplyVec4Scalar(const float a[4], const float &b, float dst[4]) {
	simde_vst1q_f32(dst,
		simde_vmulq_f32(
			simde_vld1q_f32(a),
			simde_vld1q_dup_f32(&b)));
}

inline void divideVec4(const float a[4], const float b[4], float dst[4]) {
	#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
		simde_vst1q_f32(&dst.x,
			vdivq_f32(
				simde_vld1q_f32(&a.x),
				simde_vld1q_f32(&b.x)));
	#else
		// vdivq_f32 is not defied in simde, use SSE-based replacement
		sse::divideVec4(a, b, dst);
	#endif
}

}

#endif /* XENOLITH_CORE_TYPES_XLSIMD_NEON64_H_ */
