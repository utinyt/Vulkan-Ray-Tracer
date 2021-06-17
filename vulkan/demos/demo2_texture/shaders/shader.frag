#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 col;

layout(binding = 1) uniform sampler2D texSampler;

void main(){
	col = texture(texSampler, texCoord);
}