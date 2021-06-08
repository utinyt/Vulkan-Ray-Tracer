#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UBO {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 col;
layout(location = 0) out vec3 fragColor;

void main(){
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(pos, 0.f, 1.f);
	fragColor = col;
}