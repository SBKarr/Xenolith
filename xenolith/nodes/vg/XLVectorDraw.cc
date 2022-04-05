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

#include "XLVectorDraw.h"

namespace stappler::xenolith {

constexpr size_t getMaxRecursionDepth() { return 16; }

// based on:
// http://www.antigrain.com/research/adaptive_bezier/index.html
// https://www.khronos.org/registry/OpenVG/specs/openvg_1_0_1.pdf
// http://www.diva-portal.org/smash/get/diva2:565821/FULLTEXT01.pdf
// https://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes

struct EllipseData {
	float cx;
	float cy;
	float rx;
	float ry;
	float r_sq;
	float cos_phi;
	float sin_phi;
};

static inline float draw_approx_err_sq(float e) {
	e = (1.0f / e);
	return e * e;
}

static inline float draw_dist_sq(float x1, float y1, float x2, float y2) {
	const float dx = x2 - x1, dy = y2 - y1;
	return dx * dx + dy * dy;
}

static inline float draw_angle(float v1_x, float v1_y, float v2_x, float v2_y) {
	return atan2f(v1_x * v2_y - v1_y * v2_x, v1_x * v2_x + v1_y * v2_y);
}

static void drawQuadBezierRecursive(VectorLineDrawer &drawer, float x0, float y0, float x1, float y1, float x2, float y2, size_t depth, bool fill) {
	if (depth >= getMaxRecursionDepth()) {
		return;
	}

	const float x01_mid = (x0 + x1) / 2, y01_mid = (y0 + y1) / 2; // between 0 and 1
	const float x12_mid = (x1 + x2) / 2, y12_mid = (y1 + y2) / 2; // between 1 and 2
	const float x_mid = (x01_mid + x12_mid) / 2, y_mid = (y01_mid + y12_mid) / 2; // midpoint on curve

	const float dx = x2 - x0, dy = y2 - y0;
	const float d = fabsf(((x1 - x2) * dy - (y1 - y2) * dx));

	if (d > std::numeric_limits<float>::epsilon()) { // Regular case
		const float d_sq = (d * d) / (dx * dx + dy * dy);
		if (fill && d_sq <= drawer.approxError) {
			drawer.pushLine((x1 + x_mid) / 2, (y1 + y_mid) / 2);
			fill = false;
		}
		if (d_sq <= drawer.distanceError) {
			if (drawer.angularError < std::numeric_limits<float>::epsilon()) {
				drawer.push((x1 + x_mid) / 2, (y1 + y_mid) / 2);
				return;
			} else {
				// Curvature condition  (we need it for offset curve)
				const float da = fabsf(atan2f(y2 - y1, x2 - x1) - atan2f(y1 - y0, x1 - x0));
				if (std::min(da, float(2 * M_PI - da)) < drawer.angularError) {
					drawer.push((x1 + x_mid) / 2, (y1 + y_mid) / 2);
					return;
				}
			}
		}
	} else {
		float sd;
		const float da = dx * dx + dy * dy;
		if (da == 0) {
			sd = draw_dist_sq(x0, y0, x1, y1);
		} else {
			sd = ((x1 - x0) * dx + (y1 - y0) * dy) / da;
			if (sd > 0 && sd < 1) {
				return; // degraded case
			}
			if (sd <= 0) {
				sd = draw_dist_sq(x1, y1, x0, y0);
			} else if(sd >= 1) {
				sd = draw_dist_sq(x1, y1, x2, y2);
			} else {
				sd = draw_dist_sq(x1, y1, x0 + sd * dx, y0 + sd * dy);
			}
		}
		if (fill && sd < drawer.approxError) {
			drawer.pushLine(x1, y1);
			fill = false;
		}
		if (sd < drawer.distanceError) {
			drawer.push(x1, y1);
			return;
		}
	}

	drawQuadBezierRecursive(drawer, x0, y0, x01_mid, y01_mid, x_mid, y_mid, depth + 1, fill);
	drawQuadBezierRecursive(drawer, x_mid, y_mid, x12_mid, y12_mid, x2, y2, depth + 1, fill);
}

static void drawCubicBezierRecursive(VectorLineDrawer &drawer, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, size_t depth, bool fill) {
	if (depth >= getMaxRecursionDepth()) {
		return;
	}

	const float x01_mid = (x0 + x1) / 2, y01_mid = (y0 + y1) / 2; // between 0 and 1
	const float x12_mid = (x1 + x2) / 2, y12_mid = (y1 + y2) / 2;// between 1 and 2
	const float x23_mid = (x2 + x3) / 2, y23_mid = (y2 + y3) / 2;// between 2 and 3

	const float x012_mid = (x01_mid + x12_mid) / 2, y012_mid = (y01_mid + y12_mid) / 2;// bisect midpoint in 012
	const float x123_mid = (x12_mid + x23_mid) / 2, y123_mid = (y12_mid + y23_mid) / 2;// bisect midpoint in 123

	const float x_mid = (x012_mid + x123_mid) / 2, y_mid = (y012_mid + y123_mid) / 2;// midpoint on curve

	const float dx = x3 - x0, dy = y3 - y0;
	const float d1 = fabsf(((x1 - x3) * dy - (y1 - y3) * dx));// distance factor from 0-3 to 1
	const float d2 = fabsf(((x2 - x3) * dy - (y2 - y3) * dx));// distance factor from 0-3 to 2

	const bool significantPoint1 = d1 > std::numeric_limits<float>::epsilon();
	const bool significantPoint2 = d2 > std::numeric_limits<float>::epsilon();

	if (significantPoint1 && significantPoint1) {
		const float d_sq = ((d1 + d2) * (d1 + d2)) / (dx * dx + dy * dy);

		if (fill && d_sq <= drawer.approxError) {
			drawer.pushLine(x12_mid, y12_mid);
			fill = false;
		}
		if (d_sq <= drawer.distanceError) {
			if (drawer.angularError < std::numeric_limits<float>::epsilon()) {
				drawer.push(x12_mid, y12_mid);
				return;
			}

			const float tmp = atan2(y2 - y1, x2 - x1);
			const float da1 = fabs(tmp - atan2(y1 - y0, x1 - x0));
			const float da2 = fabs(atan2(y3 - y2, x3 - x2) - tmp);
			const float da = std::min(da1, float(2 * M_PI - da1)) + std::min(da2, float(2 * M_PI - da2));
			if (da < drawer.angularError) {
				drawer.push(x12_mid, y12_mid);
				return;
			}
		}
	} else if (significantPoint1) {
		const float d_sq = (d1 * d1) / (dx * dx + dy * dy);
		if (fill && d_sq <= drawer.approxError) {
			drawer.pushLine(x12_mid, y12_mid);
			fill = false;
		}
		if (d_sq <= drawer.distanceError) {
			if (drawer.angularError < std::numeric_limits<float>::epsilon()) {
				drawer.push(x12_mid, y12_mid);
				return;
			} else {
				const float da = fabsf(atan2f(y2 - y1, x2 - x1) - atan2f(y1 - y0, x1 - x0));
				if (std::min(da, float(2 * M_PI - da)) < drawer.angularError) {
					drawer.push(x1, y1);
					drawer.push(x2, y2);
					return;
				}
			}
		}
	} else if (significantPoint2) {
		const float d_sq = (d2 * d2) / (dx * dx + dy * dy);
		if (fill && d_sq <= drawer.approxError) {
			drawer.pushLine(x12_mid, y12_mid);
			fill = false;
		}
		if (d_sq <= drawer.distanceError) {
			if (drawer.angularError < std::numeric_limits<float>::epsilon()) {
				drawer.push(x12_mid, y12_mid);
				return;
			} else {
				const float da = fabsf(atan2f(y3 - y2, x3 - x2) - atan2f(y2 - y1, x2 - x1));
				if (std::min(da, float(2 * M_PI - da)) < drawer.angularError) {
					drawer.push(x1, y1);
					drawer.push(x2, y2);
					return;
				}
			}
		}
	} else {
		float sd1, sd2;
		const float k = dx * dx + dy * dy;
		if (k == 0) {
			sd1 = draw_dist_sq(x0, y0, x1, y1);
			sd2 = draw_dist_sq(x3, y3, x2, y2);
		} else {
			sd1 = ((x1 - x0) * dx + (y1 - y0) * dy) / k;
			sd2 = ((x2 - x0) * dx + (y2 - y0) * dy) / k;
			if (sd1 > 0 && sd1 < 1 && sd2 > 0 && sd2 < 1) {
				return;
			}

			if (sd1 <= 0) {
				sd1 = draw_dist_sq(x1, y1, x0, y0);
			} else if (sd1 >= 1) {
				sd1 = draw_dist_sq(x1, y1, x3, y3);
			} else {
				sd1 = draw_dist_sq(x1, y1, x0 + d1 * dx, y0 + d1 * dy);
			}

			if (sd2 <= 0) {
				sd2 = draw_dist_sq(x2, y2, x0, y0);
			} else if (sd2 >= 1) {
				sd2 = draw_dist_sq(x2, y2, x3, y3);
			} else {
				sd2 = draw_dist_sq(x2, y2, x0 + d2 * dx, y0 + d2 * dy);
			}
		}
		if (sd1 > sd2) {
			if (fill && sd1 < drawer.approxError) {
				drawer.pushLine(x1, y1);
				fill = false;
			}
			if (sd1 < drawer.distanceError) {
				drawer.push(x1, y1);
				return;
			}
		} else {
			if (fill && sd2 < drawer.approxError) {
				drawer.pushLine(x2, y2);
				fill = false;
			}
			if (sd2 < drawer.distanceError) {
				drawer.push(x2, y2);
				return;
			}
		}
	}

	drawCubicBezierRecursive(drawer, x0, y0, x01_mid, y01_mid, x012_mid, y012_mid, x_mid, y_mid, depth + 1, fill);
	drawCubicBezierRecursive(drawer, x_mid, y_mid, x123_mid, y123_mid, x23_mid, y23_mid, x3, y3, depth + 1, fill);
}

static void drawArcRecursive(VectorLineDrawer &drawer, const EllipseData &e, float startAngle, float sweepAngle, float x0, float y0, float x1, float y1, size_t depth, bool fill) {
	if (depth >= getMaxRecursionDepth()) {
		return;
	}

	const float x01_mid = (x0 + x1) / 2, y01_mid = (y0 + y1) / 2;

	const float n_sweep = sweepAngle / 2.0f;

	const float sx_ = e.rx * cosf(startAngle + n_sweep), sy_ = e.ry * sinf(startAngle + n_sweep);
	const float sx = e.cx - (sx_ * e.cos_phi - sy_ * e.sin_phi);
	const float sy = e.cy + (sx_ * e.sin_phi + sy_ * e.cos_phi);

	const float d = draw_dist_sq(x01_mid, y01_mid, sx, sy);

	if (fill && d < drawer.approxError) {
		drawer.pushLine(sx, sy); // TODO fix line vs outline points
		fill = false;
	}

	if (d < drawer.distanceError) {
		if (drawer.angularError < std::numeric_limits<float>::epsilon()) {
			drawer.push(sx, sy);
			return;
		} else {
			const float y0_x0 = y0 / x0;
			const float y1_x1 = y1 / x1;
			const float da = fabs(atanf( e.r_sq * (y1_x1 - y0_x0) / (e.r_sq * e.r_sq * y0_x0 * y1_x1) ));
			if (da < drawer.angularError) {
				drawer.push(sx, sy);
				return;
			}
		}
	}

	drawArcRecursive(drawer, e, startAngle, n_sweep, x0, y0, sx, sy, depth + 1, fill);
	drawer.push(sx, sy);
	drawArcRecursive(drawer, e, startAngle + n_sweep, n_sweep, sx, sy, x1, y1, depth + 1, fill);
}

VectorLineDrawer::VectorLineDrawer(memory::pool_t *p) : line(p), outline(p) { }

void VectorLineDrawer::setStyle(DrawStyle s, float e, float w) {
	style = s;
	if (isStroke()) {
		approxError = draw_approx_err_sq(e);
		if (w > 1.0f) {
			distanceError = draw_approx_err_sq(e * log2f(w));
		} else {
			distanceError = draw_approx_err_sq(e);
		}
		angularError = 0.5f;
	} else {
		approxError = distanceError = draw_approx_err_sq(e);
		angularError = 0.0f;
	}
}

size_t VectorLineDrawer::capacity() const {
	return line.capacity();
}

void VectorLineDrawer::reserve(size_t size) {
	line.reserve(size);
	outline.reserve(size);
}

void VectorLineDrawer::drawLine(float x, float y) {
	push(x, y);
}

void VectorLineDrawer::drawQuadBezier(float x0, float y0, float x1, float y1, float x2, float y2) {
	drawQuadBezierRecursive(*this, x0, y0, x1, y1, x2, y2, 0, style == DrawStyle::FillAndStroke);
	push(x2, y2);
}
void VectorLineDrawer::drawCubicBezier(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
	drawCubicBezierRecursive(*this, x0, y0, x1, y1, x2, y2, x3, y3, 0, style == DrawStyle::FillAndStroke);
	push(x3, y3);
}
void VectorLineDrawer::drawArc(float x0, float y0, float rx, float ry, float phi, bool largeArc, bool sweep, float x1, float y1) {
	rx = fabsf(rx); ry = fabsf(ry);

	const float sin_phi = sinf(phi), cos_phi = cosf(phi);

	const float x01_dst = (x0 - x1) / 2, y01_dst = (y0 - y1) / 2;
	const float x1_ = cos_phi * x01_dst + sin_phi * y01_dst;
	const float y1_ = - sin_phi * x01_dst + cos_phi * y01_dst;

	const float lambda = (x1_ * x1_) / (rx * rx) + (y1_ * y1_) / (ry * ry);
	if (lambda > 1.0f) {
		rx = sqrtf(lambda) * rx; ry = sqrtf(lambda) * ry;
	}

	const float rx_y1_ = (rx * rx * y1_ * y1_);
	const float ry_x1_ = (ry * ry * x1_ * x1_);
	const float c_st = sqrtf(((rx * rx * ry * ry) - rx_y1_ - ry_x1_) / (rx_y1_ + ry_x1_));

	const float cx_ = (largeArc != sweep ? 1.0f : -1.0f) * c_st * rx * y1_ / ry;
	const float cy_ = (largeArc != sweep ? -1.0f : 1.0f) * c_st * ry * x1_ / rx;

	const float cx = cx_ * cos_phi - cy_ * sin_phi + (x0 + x1) / 2;
	const float cy = cx_ * sin_phi + cy_ * cos_phi + (y0 + y1) / 2;

	float startAngle = draw_angle(1.0f, 0.0f, - (x1_ - cx_) / rx, (y1_ - cy_) / ry);
	float sweepAngle = draw_angle((x1_ - cx_) / rx, (y1_ - cy_) / ry, (-x1_ - cx_) / rx, (-y1_ - cy_) / ry);

	sweepAngle = (largeArc)
			? std::max(fabsf(sweepAngle), float(M_PI * 2 - fabsf(sweepAngle)))
			: std::min(fabsf(sweepAngle), float(M_PI * 2 - fabsf(sweepAngle)));

	if (rx > std::numeric_limits<float>::epsilon() && ry > std::numeric_limits<float>::epsilon()) {
		const float r_avg = (rx + ry) / 2.0f;
		const float err = (r_avg - sqrtf(distanceError)) / r_avg;
		if (err > M_SQRT1_2 - std::numeric_limits<float>::epsilon()) {
			const float pts = ceilf(sweepAngle / acos((r_avg - sqrtf(distanceError)) / r_avg) / 2.0f);
			EllipseData d{ cx, cy, rx, ry, (rx * rx) / (ry * ry), cos_phi, sin_phi };

			sweepAngle = (sweep ? -1.0f : 1.0f) * sweepAngle;

			const float segmentAngle = sweepAngle / pts;

			for (uint32_t i = 0; i < uint32_t(pts); ++ i) {
				const float sx_ = d.rx * cosf(startAngle + segmentAngle), sy_ = d.ry * sinf(startAngle + segmentAngle);
				const float sx = d.cx - (sx_ * d.cos_phi - sy_ * d.sin_phi);
				const float sy = d.cy + (sx_ * d.sin_phi + sy_ * d.cos_phi);

				drawArcRecursive(*this, d, startAngle, segmentAngle, x0, y0, sx, sy, 0, style == DrawStyle::FillAndStroke);
				startAngle += segmentAngle;

				push(sx, sy);
				x0 = sx; y0 = sy;
			}

			return;
		} else {
			EllipseData d{ cx, cy, rx, ry, (rx * rx) / (ry * ry), cos_phi, sin_phi };
			drawArcRecursive(*this, d, startAngle, (sweep ? -1.0f : 1.0f) * sweepAngle, x0, y0, x1, y1, 0, style == DrawStyle::FillAndStroke);
		}
	}

	push(x1, y1);
}

void VectorLineDrawer::drawClose() {
	switch (style) {
	case DrawStyle::Fill:
	case DrawStyle::FillAndStroke:
		processIntersects(line.front().x, line.front().y);
		break;
	default: break;
	}
}

void VectorLineDrawer::clear() {
	line.clear();
	outline.clear();
}

void VectorLineDrawer::force_clear() {
	line.force_clear();
	outline.force_clear();
}

void VectorLineDrawer::pushLine(float x, float y) {
	pushLinePointWithIntersects(x, y);
}

void VectorLineDrawer::pushOutline(float x, float y) {
	outline.push_back(Vec2{x, y});
}

void VectorLineDrawer::push(float x, float y) {
	switch (style) {
	case DrawStyle::Fill:
		pushLinePointWithIntersects(x, y);
		break;
	case DrawStyle::Stroke:
		outline.push_back(Vec2{x, y});
		break;
	case DrawStyle::FillAndStroke:
		pushLinePointWithIntersects(x, y);
		outline.push_back(Vec2{x, y});
		break;
	default: break;
	}
}

void VectorLineDrawer::processIntersects(float x, float y) {
	Vec2 A(line.back().x, line.back().y);
	Vec2 B(x, y);

	for (size_t i = 0; i < line.size() - 2; ++ i) {
		auto C = Vec2(line[i].x, line[i].y);
		auto D = Vec2(line[i + 1].x, line[i + 1].y);

		Vec2::getSegmentIntersectPoint(A, B, C, D,
				[&] (const Vec2 &P, float S, float T) {
			if (S > 0.5f) { S = 1.0f - S; }
			if (T > 0.5f) { T = 1.0f - T; }

			auto dsAB = A.distanceSquared(B) * S * S;
			auto dsCD = C.distanceSquared(D) * T * T;

			if (dsAB > approxError && dsCD > approxError) {
				line.push_back(Vec2{P.x, P.y});
			}
		});
	}
}

void VectorLineDrawer::pushLinePointWithIntersects(float x, float y) {
	if (line.size() > 2) {
		processIntersects(x, y);
	}

	line.push_back(Vec2{x, y});
}

}
