#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "XLGlslVertexData.h"

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
layout (location = 2) out vec4 shadowColor;

void main() {
	const uint transformIdx = vertexBuffer[0].vertices[gl_VertexIndex].material >> 16;
	const TransformObject transform = transformObjectBuffer[1].objects[transformIdx];
	const Vertex vertex = vertexBuffer[0].vertices[gl_VertexIndex];

	gl_Position = transform.transform * vertexBuffer[0].vertices[gl_VertexIndex].pos
		* transform.mask + transform.offset;

	fragColor = vertex.color;
	fragTexCoord = vertex.tex;
	shadowColor = transform.shadow;
}
