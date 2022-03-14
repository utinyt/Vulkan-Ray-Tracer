#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "wavefront.glsl"

layout(location = 0) in vec4 inWorldPos;
layout(location = 1) in vec4 inWorldNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec4 gPos;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedo;
layout(location = 3) out float gPrimitiveID;

layout(push_constant) uniform GBufferPushConstant{
	mat4 modelMatrix;
	uint materialId;
	uint primitiveId;
};

layout(set = 0, binding = 1) readonly buffer SceneDescription {
	SceneDesc sceneDesc;
};

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(buffer_reference, scalar) readonly buffer GltfShadeMaterial {
	ShadeMaterial material[];
};

void main(){
	GltfShadeMaterial materials = GltfShadeMaterial(sceneDesc.materialAddress);
	ShadeMaterial material = materials.material[materialId];

	//fill gbuffer
	gPos = inWorldPos;
	gNormal = inWorldNormal;
	gAlbedo = material.baseColorFactor;
	if(material.baseColorTextureIndex > -1){
		gAlbedo *= texture(textures[material.baseColorTextureIndex], inUV);
	}
	gPrimitiveID = primitiveId;
}
