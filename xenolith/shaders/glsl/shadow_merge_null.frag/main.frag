#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XLGlslVertexData.h"
#include "XLGlslShadowData.h"
#include "XLGlslSdfData.h"

layout (push_constant) uniform pcb {
	uint samplerIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 2) uniform ShadowDataBuffer {
	ShadowData shadowData;
};

layout (location = 0) in vec4 fragColor;
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

void main() {
	vec4 textureColor = shadowData.globalColor;

	uint layerIdx = 0;

	for (uint i = 0; i < shadowData.ambientLightCount; ++ i) {
		textureColor += shadowData.ambientLights[i].color * shadowData.ambientLights[i].color.a.xxxx / shadowData.luminosity;
		++ layerIdx;
	}
	for (uint i = 0; i < shadowData.directLightCount; ++ i) {
		textureColor += shadowData.directLights[i].color * shadowData.directLights[i].color.a.xxxx / shadowData.luminosity;
		++ layerIdx;
	}

	outColor = vec4(textureColor.xyz, 1.0);
}
