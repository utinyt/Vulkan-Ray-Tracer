#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 modelFragPos;
layout(location = 0) out vec4 col;

void main(){
	vec3 lightPos = vec3(-2.f, 2.f, 2.f);
	vec3 fragToLight = lightPos - modelFragPos;
	float diff = dot(fragToLight, normal);
	col = vec4(vec3(diff), 1.f);
}
