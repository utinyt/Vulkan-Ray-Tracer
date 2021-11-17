#version 460
#extension GL_GOOGLE_include_directive : enable

layout(binding = 0, set = 0) uniform CameraMatrices{
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
} cam;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 normal;
layout(location = 1) out vec3 viewFragPos;
layout(location = 2) out vec3 lightPos;

void main(){
	mat4 modelView = cam.view;
	viewFragPos = (modelView * vec4(inPos, 1.f)).xyz;
	gl_Position = cam.proj * vec4(viewFragPos , 1.f);
	normal = mat3(transpose(cam.viewInverse)) * inNormal;
	lightPos = (cam.view * vec4(2.f, 2.f, 2.f, 1.f)).xyz;
}