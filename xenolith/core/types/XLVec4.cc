// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
 Copyright 2013 BlackBerry Inc.
 Copyright (c) 2017 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVec4.h"

namespace stappler::xenolith {

float Vec4::angle(const Vec4& v1, const Vec4& v2) {
	const float dx = v1.w * v2.x - v1.x * v2.w - v1.y * v2.z + v1.z * v2.y;
	const float dy = v1.w * v2.y - v1.y * v2.w - v1.z * v2.x + v1.x * v2.z;
	const float dz = v1.w * v2.z - v1.z * v2.w - v1.x * v2.y + v1.y * v2.x;

	return atan2f(sqrt(dx * dx + dy * dy + dz * dz) + MATH_FLOAT_SMALL, dot(v1, v2));
}

void Vec4::add(const Vec4& v1, const Vec4& v2, Vec4* dst) {
	assert(dst);

	dst->x = v1.x + v2.x;
	dst->y = v1.y + v2.y;
	dst->z = v1.z + v2.z;
	dst->w = v1.w + v2.w;
}

void Vec4::clamp(const Vec4& min, const Vec4& max) {
	assert(!(min.x > max.x || min.y > max.y || min.z > max.z || min.w > max.w));

	// Clamp the x value.
	if (x < min.x) {
		x = min.x;
	}
	if (x > max.x) {
		x = max.x;
	}

	// Clamp the y value.
	if (y < min.y) {
		y = min.y;
	}
	if (y > max.y) {
		y = max.y;
	}

	// Clamp the z value.
	if (z < min.z) {
		z = min.z;
	}
	if (z > max.z) {
		z = max.z;
	}

	// Clamp the z value.
	if (w < min.w) {
		w = min.w;
	}
	if (w > max.w) {
		w = max.w;
	}
}

void Vec4::clamp(const Vec4& v, const Vec4& min, const Vec4& max, Vec4* dst)
{
	assert(dst);
	assert(!(min.x > max.x || min.y > max.y || min.z > max.z || min.w > max.w));

	// Clamp the x value.
	dst->x = v.x;
	if (dst->x < min.x) {
		dst->x = min.x;
	}
	if (dst->x > max.x) {
		dst->x = max.x;
	}

	// Clamp the y value.
	dst->y = v.y;
	if (dst->y < min.y) {
		dst->y = min.y;
	}
	if (dst->y > max.y) {
		dst->y = max.y;
	}

	// Clamp the z value.
	dst->z = v.z;
	if (dst->z < min.z) {
		dst->z = min.z;
	}
	if (dst->z > max.z) {
		dst->z = max.z;
	}

	// Clamp the w value.
	dst->w = v.w;
	if (dst->w < min.w) {
		dst->w = min.w;
	}
	if (dst->w > max.w) {
		dst->w = max.w;
	}
}

float Vec4::dot(const Vec4& v1, const Vec4& v2) {
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w);
}

void Vec4::normalize() {
	float n = x * x + y * y + z * z + w * w;
	// Already normalized.
	if (n == 1.0f) {
		return;
	}

	n = sqrt(n);
	// Too close to zero.
	if (n < MATH_TOLERANCE) {
		return;
	}

	n = 1.0f / n;
	x *= n;
	y *= n;
	z *= n;
	w *= n;
}

Vec4 Vec4::getNormalized() const {
	Vec4 v(*this);
	v.normalize();
	return v;
}

void Vec4::subtract(const Vec4& v1, const Vec4& v2, Vec4* dst) {
	assert(dst);

	dst->x = v1.x - v2.x;
	dst->y = v1.y - v2.y;
	dst->z = v1.z - v2.z;
	dst->w = v1.w - v2.w;
}

}
