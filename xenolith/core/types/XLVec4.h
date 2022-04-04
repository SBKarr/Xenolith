/**
 Copyright 2013 BlackBerry Inc.
 Copyright (c) 2014-2015 Chukong Technologies
 Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 Original file from GamePlay3D: http://gameplay3d.org

 This file was modified to fit the cocos2d-x project
 */

#ifndef XENOLITH_CORE_TYPES_XLVEC4_H
#define XENOLITH_CORE_TYPES_XLVEC4_H

#include "XLMathBase.h"
#include "XLVec2.h"

namespace stappler::xenolith {

class Mat4;

class alignas(16) Vec4 {
public:
	static const Vec4 ZERO;
	static const Vec4 ONE;
	static const Vec4 UNIT_X;
	static const Vec4 UNIT_Y;
	static const Vec4 UNIT_Z;
	static const Vec4 UNIT_W;

	static float angle(const Vec4& v1, const Vec4& v2);
	static float dot(const Vec4& v1, const Vec4& v2);
	static void add(const Vec4& v1, const Vec4& v2, Vec4* dst);
	static void clamp(const Vec4& v, const Vec4& min, const Vec4& max, Vec4* dst);
	static void subtract(const Vec4& v1, const Vec4& v2, Vec4* dst);

	float x;
	float y;
	float z;
	float w;

	constexpr Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { }
	constexpr Vec4(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) { }
	constexpr Vec4(const Vec2 &origin, float zz, float ww) : x(origin.x), y(origin.y), z(zz), w(ww) { }
	constexpr Vec4(const Vec4& p1, const Vec4& p2)
	: x(p2.x - p1.x), y(p2.y - p1.y), z(p2.z - p1.z), w(p2.w - p1.w) { }

	constexpr Vec4(const Vec4& copy) = default;

	constexpr bool isZero() const { return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f; }
	constexpr bool isOne() const { return x == 1.0f && y == 1.0f && z == 1.0f && w == 1.0f; }

	constexpr void add(const Vec4& v) {
		x += v.x; y += v.y; z += v.z; w += v.w;
	}

	constexpr void subtract(const Vec4& v) {
		x -= v.x; y -= v.y; z -= v.z; w -= v.w;
	}

	void clamp(const Vec4& min, const Vec4& max);

	constexpr float distance(const Vec4& v) const {
		const float dx = v.x - x;
		const float dy = v.y - y;
		const float dz = v.z - z;
		const float dw = v.w - w;

		return sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
	}

	constexpr float distanceSquared(const Vec4& v) const {
		const float dx = v.x - x;
		const float dy = v.y - y;
		const float dz = v.z - z;
		const float dw = v.w - w;

		return (dx * dx + dy * dy + dz * dz + dw * dw);
	}

	constexpr float dot(const Vec4& v) const {
		return (x * v.x + y * v.y + z * v.z + w * v.w);
	}

	constexpr float length() const { return sqrt(x * x + y * y + z * z + w * w); }
	constexpr float lengthSquared() const { return (x * x + y * y + z * z + w * w); }
	constexpr void negate() { x = -x; y = -y; z = -z; w = -w; }

	void normalize();

	Vec4 getNormalized() const;

	void scale(float scalar) {
		simd::multiplyVec4Scalar(&this->x, scalar, &this->x);
	}

	constexpr const Vec4 operator+(const Vec4& v) const {
		Vec4 result(*this);
		result.add(v);
		return result;
	}

	constexpr Vec4& operator+=(const Vec4& v) {
		add(v);
		return *this;
	}

	constexpr const Vec4 operator-(const Vec4& v) const {
		Vec4 result(*this);
		result.subtract(v);
		return result;
	}

	constexpr Vec4& operator-=(const Vec4& v) {
		subtract(v);
		return *this;
	}

	constexpr const Vec4 operator-() const {
		Vec4 result(*this);
		result.negate();
		return result;
	}

	const Vec4 operator*(float s) const {
		Vec4 result(*this);
		result.scale(s);
		return result;
	}

	Vec4& operator*=(float s) {
		scale(s);
		return *this;
	}

	constexpr const Vec4 operator/(float s) const {
		return Vec4(this->x / s, this->y / s, this->z / s, this->w / s);
	}

	constexpr bool operator<(const Vec4& v) const {
		if (x == v.x) {
			if (y == v.y) {
				if (z < v.z) {
					if (w < v.w) {
						return w < v.w;
					}
				}
				return z < v.z;
			}
			return y < v.y;
		}
		return x < v.x;
	}

	constexpr bool operator==(const Vec4& v) const {
		return x == v.x && y == v.y && z == v.z && w == v.w;
	}

	constexpr bool operator!=(const Vec4& v) const {
		return x != v.x || y != v.y || z != v.z || w != v.w;
	}

};

constexpr const Vec4 Vec4::ZERO = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::ONE = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
constexpr const Vec4 Vec4::UNIT_X = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_Y = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_Z = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_W = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

inline Vec4 operator*(const Vec4 &l, const Vec4 &r) {
	Vec4 dst;
	simd::multiplyVec4(&l.x, &r.x, &dst.x);
	return dst;
}

inline Vec4 operator/(const Vec4 &l, const Vec4 &r) {
	Vec4 dst;
	simd::divideVec4(&l.x, &r.x, &dst.x);
	return dst;
}

inline std::basic_ostream<char> &
operator << (std::basic_ostream<char> & os, const Vec4 & vec) {
	os << "(x: " << vec.x << "; y: " << vec.y << "; z: " << vec.z << "; w: " << vec.w << ")";
	return os;
}

}

#endif // XENOLITH_CORE_TYPES_XLVEC4_H
