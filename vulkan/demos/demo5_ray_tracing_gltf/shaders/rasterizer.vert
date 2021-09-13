#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "wavefront.glsl"

layout(binding = 0) uniform CameraMatrices_{
    CameraMatrices cam;
};

layout(push_constant) uniform RasterPushConstant_{
	RasterPushConstant pc;
};

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outViewFragPos;
layout(location = 3) out vec3 outLightPos;

void main(){
	mat4 modelView = cam.view * pc.modelMatrix;
	outViewFragPos = (modelView * vec4(inPos, 1.f)).xyz;
	gl_Position = cam.proj * vec4(outViewFragPos, 1.f);
	outNormal = normalize(mat3(transpose(inverse(modelView))) * inNormal);
	outLightPos = (cam.view * vec4(pc.lightPos, 1.f)).xyz;
}