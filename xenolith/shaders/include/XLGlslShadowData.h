/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLGlslCompatibility.h"

#ifndef XL_GLSL
namespace stappler::xenolith::gl::glsl {
#endif

struct AmbientLightData {
	vec4 normal;
	color4 color;
	uint soft;
	uint padding0;
	uint padding1;
	uint padding2;
};

struct DirectLightData {
	vec4 position;
	color4 color;
	vec4 data;
};

struct ShadowData {
	color4 globalColor;
	color4 discardColor;

	vec2 shadowOffset;
	vec2 pix;

	float shadowDensity;
	uint gridSize;
	uint gridWidth;
	uint gridHeight;

	float bbOffset;
	float density;
	float shadowSdfDensity;
	float luminosity;

	uint trianglesCount;
	uint circlesCount;
	uint rectsCount;
	uint roundedRectsCount;

	uint polygonsCount;
	uint groupsCount;
	uint circleGridSizeOffset;
	uint circleGridIndexOffset;

	uint rectGridSizeOffset;
	uint rectGridIndexOffset;
	uint roundedRectGridSizeOffset;
	uint roundedRectGridIndexOffset;

	uint polygonGridSizeOffset;
	uint polygonGridIndexOffset;
	uint ambientLightCount;
	uint directLightCount;

	AmbientLightData ambientLights[16];
	DirectLightData directLights[16];
};

#ifndef XL_GLSL
}
#endif
