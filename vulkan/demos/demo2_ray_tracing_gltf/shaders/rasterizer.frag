#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "wavefront.glsl"

//input
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inViewFragPos;
layout(location = 3) in vec3 inLightPos;

//output
layout(location = 0) out vec4 col;

//push constant
layout(push_constant) uniform RasterPushConstant_{
	RasterPushConstant pc;
};

//buffers
layout(buffer_reference, scalar) buffer GltfMaterial {
	GltfShadeMaterial m[];
};

layout(set = 0, binding = 1) readonly buffer SceneDesc_ {
	SceneDesc sceneDesc;
};

void main(){
	GltfMaterial gltfMat = GltfMaterial(sceneDesc.materialAddress);
	GltfShadeMaterial mat = gltfMat.m[pc.materialId];

	//diffuse
	vec3 fragToLight = inLightPos - inViewFragPos;
	vec3 diffuse = calculateDiffuse(mat, fragToLight, inNormal);
	vec3 specular = calculateSpecular(inViewFragPos, fragToLight, inNormal);
	col = vec4(diffuse + specular, 1.f);
}
