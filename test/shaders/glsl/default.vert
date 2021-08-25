#version 450
#extension GL_ARB_separate_shader_objects : enable

struct MaterialData {
	float tilingX;
	float tilingY;
	float reflectance;
	float padding0;

	uint albedoTexture;
	uint normalTexture;
	uint roughnessTexture;
	uint padding1;
};

struct DrawData {
	uint material;
	uint transform;
	uint offset;
	uint padding;
};

layout(set = 0, binding = 2) buffer Materials { MaterialData data[]; } materials[];
layout(set = 0, binding = 2) buffer Draws { DrawData data[]; } draws[];

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragColor = colors[gl_VertexIndex];
}