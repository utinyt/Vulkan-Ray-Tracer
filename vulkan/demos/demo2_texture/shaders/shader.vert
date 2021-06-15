#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UBO {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inCol;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;

void main(){
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 0.f, 1.f);
	fragColor = inCol;
	texCoord = inTexCoord;
}