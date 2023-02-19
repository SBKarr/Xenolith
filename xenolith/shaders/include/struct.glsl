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
	vec4 discardColor;

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
	float shadowSdfDensity;

	vec2 shadowDensity;
	vec2 shadowOffset;

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
	/*vec2 e0;
	vec2 e1;
	vec2 e2;
	float e0dot;
	float e1dot;
	float e2dot;
	float s;*/
	float value;
	float opacity;
};

float triangle2d( in vec2 p, in vec2 a, in vec2 b, in vec2 c) {
	vec2 e0 = b - a;
	vec2 e1 = c - b;
	vec2 e2 = a - c;

	vec2 v0 = p - a;
	vec2 v1 = p - b;
	vec2 v2 = p - c;

	vec2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
	vec2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
	vec2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
    
    float s = e0.x*e2.y - e0.y*e2.x;
    vec2 d = min( min( vec2( dot( pq0, pq0 ), s*(v0.x*e0.y-v0.y*e0.x) ),
                       vec2( dot( pq1, pq1 ), s*(v1.x*e1.y-v1.y*e1.x) )),
                       vec2( dot( pq2, pq2 ), s*(v2.x*e2.y-v2.y*e2.x) ));

	//return -d.x * sign(d.y);
	return -sqrt(d.x)*sign(d.y);
}

float dot2(in vec3 v) { return dot(v, v); }

float triangle3d(in vec3 p, in vec2 a, in vec2 b, in vec2 c, in float value) {
	vec3 v1 = vec3(a, value);
	vec3 v2 = vec3(b, value);
	vec3 v3 = vec3(c, value);

	vec3 v21 = v2 - v1; vec3 p1 = p - v1;
	vec3 v32 = v3 - v2; vec3 p2 = p - v2;
	vec3 v13 = v1 - v3; vec3 p3 = p - v3;
	vec3 nor = cross(v21, v13);

	return sqrt((
		sign(dot(cross(v21,nor),p1)) +
		sign(dot(cross(v32,nor),p2)) +
		sign(dot(cross(v13,nor),p3)) < 2.0 )
	?
		min( min(
		dot2( v21 * clamp( dot(v21, p1) / dot2(v21), 0.0, 1.0 ) - p1), 
		dot2( v32 * clamp( dot(v32, p2) / dot2(v32), 0.0, 1.0 ) - p2) ), 
		dot2( v13 * clamp( dot(v13, p3) / dot2(v13), 0.0, 1.0 ) - p3) )
	:
		dot(nor, p1) * dot(nor, p1) / dot2(nor)
	);
}
