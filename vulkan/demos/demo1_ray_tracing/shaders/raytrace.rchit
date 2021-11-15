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

layout(buffer_reference, scalar) buffer Vertices {
	Vertex v[]; //position
};
layout(buffer_reference, scalar) buffer Indices {
	ivec3 i[]; //triangle indices
};
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 1, scalar) buffer SceneDesc_ {
	SceneDesc i[];
} sceneDesc;

layout(push_constant) uniform Constants{
	vec4 clearColor;
	vec3 lightPosition;
	float lightIntensity;
	int lightType;
} constants;

void main() {
	SceneDesc objInstance = sceneDesc.i[gl_InstanceCustomIndexEXT]; //gl_InstanceCustomIndexEXT - object that was hit
	Indices indices = Indices(objInstance.indexAddress);
	Vertices vertices = Vertices(objInstance.vertexAddress);

	//indices of the triangle
	ivec3 ind = indices.i[gl_PrimitiveID]; // ID of triangle that was hit

	//vertices of the triangle
	Vertex v0 = vertices.v[ind.x];
	Vertex v1 = vertices.v[ind.y];
	Vertex v2 = vertices.v[ind.z];

	//interpolate normal using barycentric coordinates
	const vec3 barycentrics = vec3(1.0 - attrib.x - attrib.y, attrib.x, attrib.y);
	vec3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
	normal = normalize(vec3(objInstance.transformIT * vec4(normal, 0.0)));

	//this could have precision issue if the hit point is very far
	//vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitEXT;
	vec3 worldPos = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
	worldPos = vec3(objInstance.transform * vec4(worldPos, 1.0));

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
