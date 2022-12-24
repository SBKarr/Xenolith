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

struct AmbientLightData {
	vec4 normal;
	vec4 color;
	uint soft;
	uint padding0;
	uint padding1;
	uint padding2;
};

struct DirectLightData {
	vec4 position;
	vec4 color;
	vec4 data;
};

struct ShadowData {
	vec4 globalColor;

	uint trianglesCount;
	uint gridSize;
	uint gridWidth;
	uint gridHeight;

	uint ambientLightCount;
	uint directLightCount;
	float bbOffset;
	float density;

	vec2 pix;
	float luminosity;
	float shadowDensity;

	AmbientLightData ambientLights[16];
	DirectLightData directLights[16];
};

struct IndexData {
	uint a;
	uint b;
	uint c;
	uint transform;
	float value;
	float opacity;
};

struct TriangleData {
	vec2 bbMin;
	vec2 bbMax;
	vec2 a;
	vec2 b;
	vec2 c;
	float value;
	float opacity;
};
