#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 fragColor;

vec2 pos[3] = vec2[](
	vec2(0.f, -0.5f),
	vec2(-0.5f, 0.5f),
	vec2(0.5f, 0.5f)
);

vec3 colors[3] = vec3[](
	vec3(1.f, 0.f, 0.f),
	vec3(0.f, 1.f, 0.f),
	vec3(0.f, 0.f, 1.f)
);

void main(){
	gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
	fragColor = colors[gl_VertexIndex];
}