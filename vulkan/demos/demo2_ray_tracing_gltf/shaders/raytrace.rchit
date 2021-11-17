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
hitAttributeEXT vec2 attrib;

layout(buffer_reference, scalar) readonly buffer Vertices {
	vec3 v[]; //position
};
layout(buffer_reference, scalar) readonly buffer Normals {
	vec3 n[]; //normals
};
layout(buffer_reference, scalar) readonly buffer Indices {
	uint i[]; //triangle indices
};
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) readonly  buffer Primitives {
	Primitive prim[]; //triangle indices
};
layout(binding = 1, set = 1, scalar) readonly buffer Scene {
	SceneDesc sceneDesc;
};

layout(push_constant) uniform Constants{
	vec4 clearColor;
	vec3 lightPosition;
	float lightIntensity;
	int lightType;
} constants;

void main() {
	//SceneDesc objInstance = sceneDesc.i[0]; //gl_InstanceCustomIndexEXT - object that was hit
	Indices indices = Indices(sceneDesc.indexAddress);
	Vertices vertices = Vertices(sceneDesc.vertexAddress);
	Normals normals = Normals(sceneDesc.normalAddress);

	Primitive primInfo = prim[gl_InstanceCustomIndexEXT];
	uint indexOffset = primInfo.firstIndex + (3 * gl_PrimitiveID);
	uint vertexOffset = primInfo.vertexStart;

	ivec3 triangleIndex = ivec3(indices.i[indexOffset + 0], indices.i[indexOffset + 1], indices.i[indexOffset + 2]);
	
	const vec3 barycentrics = vec3(1.0 - attrib.x - attrib.y, attrib.x, attrib.y);

	//this could have precision issue if the hit point is very far
	//vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitEXT;
	//vertices of the triangle
	vec3 v0 = vertices.v[triangleIndex.x];
	vec3 v1 = vertices.v[triangleIndex.y];
	vec3 v2 = vertices.v[triangleIndex.z];
	vec3 worldPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
	worldPos = vec3(sceneDesc.transform * vec4(worldPos, 1.0));

	//normals of the triangle
	vec3 n0 = normals.n[triangleIndex.x];
	vec3 n1 = normals.n[triangleIndex.y];
	vec3 n2 = normals.n[triangleIndex.z];
	vec3 normal = n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z;
	normal = normalize(vec3(sceneDesc.transformIT * vec4(normal, 0.0)));

	vec3 L;
	//point light
	if(constants.lightType == 0){
		vec3 lightDir = constants.lightPosition - worldPos;
		L = normalize(lightDir);
	}else{
		L = normalize(constants.lightPosition);
	}

	float NL = max(dot(normal, L), 0.2);

	prd.hitValue = vec3(NL, NL, NL);
}
