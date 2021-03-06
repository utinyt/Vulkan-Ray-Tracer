#version 450
//#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 col;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main(){
	col = texture(tex, outUV);
}
