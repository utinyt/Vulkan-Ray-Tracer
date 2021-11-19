#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "wavefront.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 viewFragPos;
layout(location = 3) in vec3 viewLightPos;
layout(location = 0) out vec4 col;

layout(push_constant) uniform RasterPushConstant{
	mat4 modelMatrix;
	vec3 lightPos;
	uint materialId;
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

	//diffuse color
	vec3 fragToLight = viewLightPos - viewFragPos;
	fragToLight = normalize(fragToLight);
	float diff = max(dot(fragToLight, inNormal), 0);
	col = vec4(vec3(diff) *material.baseColorFactor.xyz , 1.f);
	if(material.baseColorTextureIndex > -1){
		col.xyz *= texture(textures[material.baseColorTextureIndex], inUV).xyz;
	}

	//specular
	vec3 fragToView = normalize(-viewFragPos);
	vec3 halfwayVector = normalize(fragToView + fragToLight);
	float spec = pow(max(dot(halfwayVector, inNormal), 0), 32);

	col.xyz += vec3(spec);
}
