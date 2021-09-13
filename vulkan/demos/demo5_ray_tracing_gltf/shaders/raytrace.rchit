#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#extension GL_GOOGLE_include_directive : enable
#include "ray_common.glsl"
#include "wavefront.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;
hitAttributeEXT vec2 attrib;


//buffer references
layout(buffer_reference, scalar) readonly buffer Vertices {
	vec3 v[]; //position
};
layout(buffer_reference, scalar) readonly buffer Indices {
	uint i[]; //triangle indices
};
layout(buffer_reference, scalar) readonly buffer Normals {
	vec3 n[]; //normal
};
layout(buffer_reference, scalar) readonly buffer TexCoords {
	vec2 t[]; //texcoord
};
layout(buffer_reference, scalar) readonly buffer Materials {
	GltfShadeMaterial m[]; //material
};

//rt descriptor set 0
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) readonly buffer InstanceInfo{
	PrimMeshInfo primInfo[];
};

//rt descriptor set 1
layout(binding = 1, set = 1) readonly buffer SceneDesc_ {
	SceneDesc sceneDesc;
};

//push constant
layout(push_constant) uniform Constants{
	vec4 clearColor;
	vec3 lightPosition;
	float lightIntensity;
	int lightType;
} constants;

void main() {
	//retrieve the primitive mesh buffer info
	PrimMeshInfo pinfo = primInfo[gl_InstanceCustomIndexEXT];

	//get the first index for this mesh
	uint indexOffset = pinfo.indexOffset + (3 * gl_PrimitiveID);
	uint vertexOffset = pinfo.vertexOffset;
	uint matIndex = max(0, pinfo.materialIndex);

	Vertices	vertices	= Vertices(sceneDesc.vertexAddress);
	Indices		indices		= Indices(sceneDesc.indexAddress);
	Normals		normals		= Normals(sceneDesc.normalAddress);
	TexCoords	texCoords	= TexCoords(sceneDesc.uvAddress);
	Materials	materials	= Materials(sceneDesc.materialAddress);

	//local indices
	ivec3 triangleIndex = ivec3(indices.i[indexOffset + 0], indices.i[indexOffset + 1], indices.i[indexOffset + 2]);
	//global indices
	triangleIndex += ivec3(vertexOffset);

	const vec3 barycentric = vec3(1.0 - attrib.x - attrib.y, attrib.x, attrib.y);

	//vertex of the triangleIndex
	const vec3 pos0				= vertices.v[triangleIndex.x];
	const vec3 pos1				= vertices.v[triangleIndex.y];
	const vec3 pos2				= vertices.v[triangleIndex.z];
	const vec3 position			= pos0 * barycentric.x + pos1 * barycentric.y + pos2 * barycentric.z;
	const vec3 worldPosition	= vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));

	//normal
	const vec3	normal0			= normals.n[triangleIndex.x];
	const vec3	normal1			= normals.n[triangleIndex.y];
	const vec3	normal2			= normals.n[triangleIndex.z];
	vec3		normal			= normalize(normal0 * barycentric.x + normal1 * barycentric.y + normal2 * barycentric.z);
	const vec3	worldNormal		= normalize(normal * gl_WorldToObjectEXT).xyz;

	//texcoord
	const vec2 uv0				= texCoords.t[triangleIndex.x];
	const vec2 uv1				= texCoords.t[triangleIndex.y];
	const vec2 uv2				= texCoords.t[triangleIndex.z];
	const vec2 texcoord0		= uv0 * barycentric.x + uv1 * barycentric.y + uv2 * barycentric.z;

	vec3 L;
	float lightIntensity = constants.lightIntensity;
	float lightDistance = 100000.0;
	//point light
	if(constants.lightType == 0){
		vec3 lightDir = constants.lightPosition - worldPosition;
		lightDistance = length(lightDir);
		lightIntensity = constants.lightIntensity / (lightDistance * lightDistance);
		L = normalize(lightDir);
	}else{
		L = normalize(constants.lightPosition);
	}

	//material
	GltfShadeMaterial mat = materials.m[matIndex];

	//diffuse
	vec3 diffuse = calculateDiffuse(mat, L, worldNormal);

	vec3 specular = vec3(0.0);
	float attenuation = 1;

	if(dot(worldNormal, L) > 0){
		float tMin = 0.001;
		float tMax = lightDistance;
		vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
		vec3 rayDir = L;
		uint flag = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		isShadowed = true;

		traceRayEXT(
			topLevelAS,
			flag,
			0xFF,
			0,
			0,
			1,
			origin,
			tMin,
			rayDir,
			tMax,
			1
		);

		if(isShadowed){
			attenuation = 0.3;
		}
		else{
			//specular
			specular = calculateSpecular(gl_WorldRayDirectionEXT, L, worldNormal);
		}
	}

	prd.hitValue = vec3(lightIntensity * attenuation * (diffuse + specular));
}
