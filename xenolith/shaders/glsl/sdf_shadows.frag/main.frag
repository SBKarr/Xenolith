#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XLGlslVertexData.h"
#include "XLGlslShadowData.h"
#include "XLGlslSdfData.h"

#define OUTPUT_BUFFER_LAYOUT_SIZE 7

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
} trianglesBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer GridSizeBuffer {
	uint grid[];
} gridSizeBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer GridIndexesBuffer {
	uint index[];
} gridIndexBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer CirclesBuffer {
	Circle2DData circles[];
} circlesBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer RectsBuffer {
	Rect2DData rects[];
} rectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer RoundedRectsBuffer {
	RoundedRect2DData rects[];
} roundedRectsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(set = 0, binding = 3) buffer PolygonBuffer {
	Polygon2DData polygons[];
} polygonsBuffer[OUTPUT_BUFFER_LAYOUT_SIZE];

layout(input_attachment_index = 0, set = 0, binding = 4) uniform subpassInput inputDepth;

layout(set = 0, binding = 5) uniform texture2D sdfImage;

layout(set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

uint s_cellIdx;

#define GAUSSIAN_CONST -6.2383246250

uint hit(in vec2 p, float h) {
	uint idx;
	uint targetOffset = s_cellIdx * shadowData.trianglesCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (trianglesBuffer[0].triangles[idx].value > h + 0.1) {
			if (all(greaterThan(p, trianglesBuffer[0].triangles[idx].bbMin)) && all(lessThan(p, trianglesBuffer[0].triangles[idx].bbMax))) {
				return 1;
			}
		}
	}

	targetOffset = shadowData.circleGridIndexOffset + s_cellIdx * shadowData.circlesCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[shadowData.circleGridSizeOffset + s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (circlesBuffer[3].circles[idx].value > h + 0.1) {
			if (all(greaterThan(p, circlesBuffer[3].circles[idx].bbMin)) && all(lessThan(p, circlesBuffer[3].circles[idx].bbMax))) {
				return 1;
			}
		}
	}

	targetOffset = shadowData.rectGridIndexOffset + s_cellIdx * shadowData.rectsCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[shadowData.rectGridSizeOffset + s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (rectsBuffer[4].rects[idx].value > h + 0.1) {
			if (all(greaterThan(p, rectsBuffer[4].rects[idx].bbMin)) && all(lessThan(p, rectsBuffer[4].rects[idx].bbMax))) {
				return 1;
			}
		}
	}
	
	targetOffset = shadowData.roundedRectGridIndexOffset + s_cellIdx * shadowData.roundedRectsCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[shadowData.roundedRectGridSizeOffset + s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (roundedRectsBuffer[5].rects[idx].value > h + 0.1) {
			if (all(greaterThan(p, roundedRectsBuffer[5].rects[idx].bbMin)) && all(lessThan(p, roundedRectsBuffer[5].rects[idx].bbMax))) {
				return 1;
			}
		}
	}
	
	targetOffset = shadowData.polygonGridIndexOffset + s_cellIdx * shadowData.polygonsCount;
	for (uint i = 0; i < gridSizeBuffer[1].grid[shadowData.polygonGridSizeOffset + s_cellIdx]; ++ i) {
		idx = gridIndexBuffer[2].index[targetOffset + i];
		// value кодируется как f32, а h как f16, без коррекции точности будет мерцать
		if (polygonsBuffer[6].polygons[idx].value > h + 0.1) {
			if (all(greaterThan(p, polygonsBuffer[6].polygons[idx].bbMin)) && all(lessThan(p, polygonsBuffer[6].polygons[idx].bbMax))) {
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
		outColor = shadowData.discardColor;
		//outColor = vec4(0.75, 1, 1, 1);
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
		outColor = shadowData.discardColor;
		//outColor = vec4(1, 0.75, 1, 1);
	}

}
