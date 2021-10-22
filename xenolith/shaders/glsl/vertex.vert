#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
	uint object;
	uint material;
};

layout (set = 0, binding = 1) readonly buffer Vertices {
	Vertex vertices[];
};

layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex].pos.xy, 0.0, 1.0);
	fragColor = vertices[gl_VertexIndex].color;
	gl_PointSize = 4.0;
}
