#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XLGlslVertexData.h"
#include "XLGlslShadowData.h"
#include "XLGlslSdfData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;

layout (push_constant) uniform pcb {
	uint samplerIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 2) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

layout(set = 0, binding = 3) buffer TrianglesBuffer {
	Triangle2DData triangles[];
} trianglesBuffer[3];

layout(set = 0, binding = 3) buffer GridSizeBuffer {
	uint grid[];
} gridSizeBuffer[3];

layout(set = 0, binding = 3) buffer GridIndexesBuffer {
	uint index[];
} gridIndexBuffer[3];

layout(input_attachment_index = 0, set = 0, binding = 4) uniform subpassInput inputDepth;

layout(set = 0, binding = 5) uniform texture2D sdfImage;

layout(set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

uint s_cellIdx;

#define GAUSSIAN_CONST -6.2383246250

float map(in vec3 p) {
	float value = 100.0;

	uint targetOffset = s_cellIdx * shadowData.trianglesCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[s_cellIdx]; ++ i) {
		Triangle2DData t = trianglesBuffer[0].triangles[gridIndexBuffer[2].index[targetOffset + i]];
		value = min(value, triangle3d(p, t.a, t.b, t.c, t.value));
	}
	return value;
}

float map2d(in vec2 p, in vec2 n, in float k) {
	float value = 100.0;
	float sdf;
	float max = 100.0;
	uint index;

	uint targetOffset = s_cellIdx * shadowData.trianglesCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[s_cellIdx]; ++ i) {
		index = gridIndexBuffer[2].index[targetOffset + i];
		Triangle2DData t = trianglesBuffer[0].triangles[index];
		sdf = triangle2d(p + n * trianglesBuffer[0].triangles[index].value, t.a, t.b, t.c);
		if (value > sdf) {
			max = trianglesBuffer[0].triangles[index].value * k * k * 1.5;
			value = sdf;
		}
	}

	value = clamp(value / max, 0.0, 1.0);
	return 1.0 - exp(value * value * GAUSSIAN_CONST);
}

float softshadow(in vec3 ro, in vec3 rd, in float k) {
	float res = 1.0;
	float t = 0.002;
	float h = 1.0;
	for (int i = 0; i < 1000; i++) {
		h = map(ro + rd * t);
		if (h < 0.0001) {
			return 0.0;
		}
		res = min(res, h / (t * k * 0.5));
		t += h;
		if (t > 100.0) {
			break;
		}
	}
	return clamp(res, 0.0, 1.0);
}

float softshadow2(in vec3 ro, in vec3 rd, in float k) {
	float res = 1.0;
	float t = 0.002;
	float h = 1.0;
	for (int i = 0; i < 1000; i++) {
		h = map(ro + rd * t) + t * (k * 0.5);
		if (h < 0.001) {
			return 0.0;
		}
		res = min(res, h / (t * k));
		t += h * inversesqrt(t + 1.0);
		if (t > 100.0) {
			break;
		}
	}
	return clamp(res, 0.0, 1.0);
}

uint hit(in vec2 p, float h) {
	uint idx;
	uint targetOffset = s_cellIdx * shadowData.trianglesCount;

	for (uint i = 0; i < gridSizeBuffer[1].grid[s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (trianglesBuffer[0].triangles[idx].value > h + 0.02) {
			if (all(greaterThan(p, trianglesBuffer[0].triangles[idx].bbMin)) && all(lessThan(p, trianglesBuffer[0].triangles[idx].bbMax))) {
				return 1;
			}
		}
	}
	return 0;
}

void main() {
	vec2 coords = (vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) / shadowData.pix) * shadowData.shadowDensity;
	uvec2 cellIdx = uvec2(floor(coords)) / shadowData.gridSize;
	s_cellIdx = cellIdx.y * (shadowData.gridWidth) + cellIdx.x;

	float depth = subpassLoad(inputDepth).r;
	vec2 sdfValue = texture(sampler2D(sdfImage, immutableSamplers[pushConstants.samplerIdx]), fragTexCoord).xy;
	if (sdfValue.y < depth + 0.1) {
		//outColor = vec4(1.0, 0.75, 1.0, 1.0);
		outColor = shadowData.discardColor;
		return;
	}

	if (hit(coords, depth) == 1) {
		vec4 textureColor = shadowData.globalColor;
		for (uint i = 0; i < shadowData.ambientLightCount; ++ i) {
			float k = shadowData.ambientLights[i].normal.w;

			vec4 targetSdf = texture(sampler2D(sdfImage, immutableSamplers[pushConstants.samplerIdx]),
				fragTexCoord + shadowData.ambientLights[i].normal.xy * sdfValue.y);
			float targetValue = targetSdf.x;
			float sdf = clamp( (max(targetValue, sdfValue.y - depth) - depth - sdfValue.y * 0.5) / ((sdfValue.y * 0.55 - depth) * k * k), 0.0, 1.0);

			textureColor += shadowData.ambientLights[i].color
				* shadowData.ambientLights[i].color.a.xxxx
				* (1.0 - exp(sdf * sdf * sdf * GAUSSIAN_CONST)).xxxx
				* shadowData.luminosity;
		}

		outColor = vec4(textureColor.xyz, 1.0);
	} else {
		//outColor = vec4(0.75, 1.0, 1.0, 1.0);
		outColor = shadowData.discardColor;
	}

}
