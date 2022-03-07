#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint object;
	uint material;
};

struct Material {
	uint samplerIdx;
	uint textureIdx;
	uint setIdx;
	uint padding0;
};

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 1) readonly buffer Vertices {
	Vertex vertices[];
};

layout (set = 0, binding = 2) readonly buffer Materials {
	Material materials[];
};

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex].pos.xyz, 1.0);
	fragColor = vertices[gl_VertexIndex].color;
	fragTexCoord = vertices[gl_VertexIndex].tex;
}
