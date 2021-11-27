#version 450
//#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 col;

layout(binding = 0) uniform sampler2D lighting;
layout(binding = 1) uniform sampler2D albedo;

void main(){
	col = texture(lighting, outUV);
	//col.xyz *= col.xyz;
	col.xyz *= texture(albedo, outUV).xyz;
}
