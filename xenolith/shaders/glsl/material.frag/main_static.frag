#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Material {
	uint samplerIdx;
	uint imageIdx;
	uint setIdx;
	uint padding0;
};

layout (constant_id = 0) const int SAMPLERS_ARRAY_SIZE = 1;
layout (constant_id = 1) const int IMAGES_ARRAY_SIZE = 1;
layout (constant_id = 2) const int TARGET_SAMPLER = 0;

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
layout (location = 0) out vec4 outColor;
layout (location = 1) in vec2 fragTexCoord;

void main() {
	vec4 textureColor = texture(
		sampler2D(
			images[0],
			immutableSamplers[TARGET_SAMPLER]
		), fragTexCoord);
	outColor = fragColor * textureColor;
}
