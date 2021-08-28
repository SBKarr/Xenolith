#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
};

layout(set = 2, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
};

layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = vertices[gl_VertexIndex].pos;
	fragColor = vertices[gl_VertexIndex].color;
}
