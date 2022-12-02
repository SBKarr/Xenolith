#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint material;
	uint object;
};

struct Material {
	uint samplerIdx;
	uint textureIdx;
	uint setIdx;
	uint padding0;
};

struct TransformObject {
	mat4 transform;
	vec4 mask;
	vec4 offset;
};

layout (push_constant) uniform pcb {
	uint materialIdx;
	uint padding0;
	uint padding1;
} pushConstants;

layout (set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
} vertexBuffer[2];

layout (set = 0, binding = 0) readonly buffer TransformObjects {
	TransformObject objects[];
} transformObjectBuffer[2];

layout (set = 0, binding = 1) readonly buffer Materials {
	Material materials[];
};

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;

void main() {
	uint transformIdx = vertexBuffer[0].vertices[gl_VertexIndex].material >> 16;

	gl_Position = transformObjectBuffer[1].objects[transformIdx].transform * vertexBuffer[0].vertices[gl_VertexIndex].pos
		* transformObjectBuffer[1].objects[transformIdx].mask + transformObjectBuffer[1].objects[transformIdx].offset;

	fragColor = vertexBuffer[0].vertices[gl_VertexIndex].color;
	fragTexCoord = vertexBuffer[0].vertices[gl_VertexIndex].tex;
}
