// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/**

Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2014 Chukong Technologies
Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLGeometry.h"

namespace stappler::xenolith {

bool Rect::containsPoint(const Vec2& point) const {
	bool bRet = false;

	if (point.x >= getMinX() && point.x <= getMaxX() && point.y >= getMinY() && point.y <= getMaxY()) {
		bRet = true;
	}

	return bRet;
}

bool Rect::intersectsRect(const Rect& rect) const {
	return !( getMaxX() < rect.getMinX() ||
			rect.getMaxX() < getMinX() ||
			getMaxY() < rect.getMinY() ||
			rect.getMaxY() < getMinY());
}

bool Rect::intersectsCircle(const Vec2 &center, float radius) const {
	Vec2 rectangleCenter((origin.x + size.width / 2),
			(origin.y + size.height / 2));

	float w = size.width / 2;
	float h = size.height / 2;

	float dx = fabs(center.x - rectangleCenter.x);
	float dy = fabs(center.y - rectangleCenter.y);

	if (dx > (radius + w) || dy > (radius + h)) {
		return false;
	}

	Vec2 circleDistance(fabs(center.x - origin.x - w),
			fabs(center.y - origin.y - h));

	if (circleDistance.x <= (w)) {
		return true;
	}

	if (circleDistance.y <= (h)) {
		return true;
	}

	float cornerDistanceSq = powf(circleDistance.x - w, 2) + powf(circleDistance.y - h, 2);

	return (cornerDistanceSq <= (powf(radius, 2)));
}

void Rect::merge(const Rect& rect) {
	float top1 = getMaxY();
	float left1 = getMinX();
	float right1 = getMaxX();
	float bottom1 = getMinY();

	float top2 = rect.getMaxY();
	float left2 = rect.getMinX();
	float right2 = rect.getMaxX();
	float bottom2 = rect.getMinY();
	origin.x = std::min(left1, left2);
	origin.y = std::min(bottom1, bottom2);
	size.width = std::max(right1, right2) - origin.x;
	size.height = std::max(top1, top2) - origin.y;
}

Rect Rect::unionWithRect(const Rect & rect) const {
	float thisLeftX = origin.x;
	float thisRightX = origin.x + size.width;
	float thisTopY = origin.y + size.height;
	float thisBottomY = origin.y;

	if (thisRightX < thisLeftX) {
		std::swap(thisRightX, thisLeftX);   // This rect has negative width
	}

	if (thisTopY < thisBottomY) {
		std::swap(thisTopY, thisBottomY);   // This rect has negative height
	}

	float otherLeftX = rect.origin.x;
	float otherRightX = rect.origin.x + rect.size.width;
	float otherTopY = rect.origin.y + rect.size.height;
	float otherBottomY = rect.origin.y;

	if (otherRightX < otherLeftX) {
		std::swap(otherRightX, otherLeftX);   // Other rect has negative width
	}

	if (otherTopY < otherBottomY) {
		std::swap(otherTopY, otherBottomY);   // Other rect has negative height
	}

	float combinedLeftX = std::min(thisLeftX, otherLeftX);
	float combinedRightX = std::max(thisRightX, otherRightX);
	float combinedTopY = std::max(thisTopY, otherTopY);
	float combinedBottomY = std::min(thisBottomY, otherBottomY);

	return Rect(combinedLeftX, combinedBottomY, combinedRightX - combinedLeftX, combinedTopY - combinedBottomY);
}

Rect TransformRect(const Rect& rect, const Mat4& transform) {
	const float top = rect.getMinY();
	const float left = rect.getMinX();
	const float right = rect.getMaxX();
	const float bottom = rect.getMaxY();

	Vec2 topLeft(left, top);
	Vec2 topRight(right, top);
	Vec2 bottomLeft(left, bottom);
	Vec2 bottomRight(right, bottom);

	transform.transformPoint(&topLeft);
	transform.transformPoint(&topRight);
	transform.transformPoint(&bottomLeft);
	transform.transformPoint(&bottomRight);

	const float minX = min(min(topLeft.x, topRight.x), min(bottomLeft.x, bottomRight.x));
	const float maxX = max(max(topLeft.x, topRight.x), max(bottomLeft.x, bottomRight.x));
	const float minY = min(min(topLeft.y, topRight.y), min(bottomLeft.y, bottomRight.y));
	const float maxY = max(max(topLeft.y, topRight.y), max(bottomLeft.y, bottomRight.y));

	return Rect(minX, minY, (maxX - minX), (maxY - minY));
}

}
