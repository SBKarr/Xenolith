#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XLGlslVertexData.h"

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 2;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 128;

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 1) readonly buffer Materials {
	Material materials[];
};

layout (set = 1, binding = 0) uniform sampler immutableSamplers[SAMPLERS_ARRAY_SIZE];
layout (set = 1, binding = 1) uniform texture2D images[IMAGES_ARRAY_SIZE];

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

void main() {
	vec4 textureColor = texture(
		sampler2D(
			images[materials[pushConstants.materialIdx].samplerImageIdx & 0xFFFF],
			immutableSamplers[materials[pushConstants.materialIdx].samplerImageIdx >> 16]
		), fragTexCoord);
	outColor = fragColor * textureColor;
}
