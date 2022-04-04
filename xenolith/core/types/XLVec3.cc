// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**
 Copyright 2013 BlackBerry Inc.
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

#include "XLVec3.h"

namespace stappler::xenolith {

float Vec3::angle(const Vec3& v1, const Vec3& v2) {
	const float dx = v1.y * v2.z - v1.z * v2.y;
	const float dy = v1.z * v2.x - v1.x * v2.z;
	const float dz = v1.x * v2.y - v1.y * v2.x;

	return atan2f(sqrt(dx * dx + dy * dy + dz * dz) + MATH_FLOAT_SMALL, dot(v1, v2));
}

void Vec3::clamp(const Vec3& min, const Vec3& max) {
	assert(!(min.x > max.x || min.y > max.y || min.z > max.z));

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
}

void Vec3::clamp(const Vec3& v, const Vec3& min, const Vec3& max, Vec3* dst) {
	assert(dst);
	assert(!(min.x > max.x || min.y > max.y || min.z > max.z));

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
}

float Vec3::distance(const Vec3& v) const {
	const float dx = v.x - x;
	const float dy = v.y - y;
	const float dz = v.z - z;

	return sqrt(dx * dx + dy * dy + dz * dz);
}

float Vec3::distanceSquared(const Vec3& v) const {
	const float dx = v.x - x;
	const float dy = v.y - y;
	const float dz = v.z - z;

	return (dx * dx + dy * dy + dz * dz);
}

void Vec3::normalize() {
	float n = x * x + y * y + z * z;
	// Already normalized.
	if (n == 1.0f)
	return;

	n = sqrt(n);
	// Too close to zero.
	if (n < MATH_TOLERANCE)
	return;

	n = 1.0f / n;
	x *= n;
	y *= n;
	z *= n;
}

Vec3 Vec3::getNormalized() const {
	Vec3 v(*this);
	v.normalize();
	return v;
}

void Vec3::smooth(const Vec3& target, float elapsedTime, float responseTime) {
	if (elapsedTime > 0) {
		*this += (target - *this) * (elapsedTime / (elapsedTime + responseTime));
	}
}

}
