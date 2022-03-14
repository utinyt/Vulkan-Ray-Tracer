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
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec4 outWorldNormal;
layout(location = 2) out vec2 outUV;

layout(push_constant) uniform GBufferPushConstant{
	mat4 modelMatrix;
	uint materialId;
	uint primitiveId;
};

void main(){
	outWorldPos = modelMatrix * vec4(inPos, 1.f);
	gl_Position = cam.proj * cam.view * outWorldPos;
	outWorldNormal = vec4(mat3(transpose(inverse(modelMatrix))) * inNormal, 0.f);
	outUV = inUV;
}