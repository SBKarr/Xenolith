#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Vertex {
	vec4 pos;
	vec4 color;
	vec2 tex;
};

layout(set = 0, binding = 0) readonly buffer Vertices {
	Vertex vertices[];
};

vec2 positions[4] = vec2[](
	vec2(-0.5, 0.5),
	vec2(-0.5, -0.5),
	vec2(0.5, 0.5),
	vec2(0.5, -0.5)
);

vec4 colors[4] = vec4[](
	vec4(1.0, 0.0, 0.0, 1.0),
	vec4(0.0, 1.0, 0.0, 1.0),
	vec4(0.0, 0.0, 1.0, 1.0),
	vec4(1.0, 1.0, 1.0, 1.0)
);

layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex].pos.xy, 0.0, 1.0);
	fragColor = vertices[gl_VertexIndex].color;
	//gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	//fragColor = colors[gl_VertexIndex];
	//gl_PointSize = 4.0;
}
