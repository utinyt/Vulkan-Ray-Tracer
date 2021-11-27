#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#include "ray_common.glsl"
#include "wavefront.glsl"
#include "random.glsl"
#include "sampling.glsl"

/*
* payloads
*/
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool prdDirectLightConnection;

hitAttributeEXT vec2 attrib;

/*
* buffer references
*/
layout(buffer_reference, scalar) readonly buffer Vertices {
	vec3 v[]; //position
};
layout(buffer_reference, scalar) readonly buffer Normals {
	vec3 n[]; //normals
};
layout(buffer_reference, scalar) readonly buffer Texcoord0s {
	vec2 t[]; //normals
};
layout(buffer_reference, scalar) readonly buffer Indices {
	uint i[]; //triangle indices
};
layout(buffer_reference, scalar) readonly buffer MaterialIndices {
	uint i[]; //triangle indices
};
layout(buffer_reference, scalar) readonly buffer Materials {
	ShadeMaterial m[]; //triangle indices
};

/*
* descriptors
*/
//ray tracing descriptors
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 4, set = 0) readonly  buffer Primitives {
	Primitive prim[]; //triangle indices
};

//shared descriptors
layout(binding = 1, set = 1, scalar) readonly buffer Scene {
	SceneDesc sceneDesc;
};
layout(binding = 2, set = 1) uniform sampler2D textures[];

//push_constant
layout(push_constant) uniform PushConstant {
	RtPushConstant pc;
};

void main() {
	Indices indices = Indices(sceneDesc.indexAddress);
	Vertices vertices = Vertices(sceneDesc.vertexAddress);
	Normals normals = Normals(sceneDesc.normalAddress);
	Texcoord0s texcoord0s = Texcoord0s(sceneDesc.uvAddress);
	MaterialIndices materialIndices = MaterialIndices(sceneDesc.materialIndicesAddress);
	Materials materials = Materials(sceneDesc.materialAddress);

	Primitive primInfo = prim[gl_InstanceCustomIndexEXT];
	uint indexOffset = primInfo.firstIndex + (3 * gl_PrimitiveID);
	uint vertexOffset = primInfo.vertexStart;
	uint materialIndex = primInfo.materialIndex;

	ivec3 triangleIndex = ivec3(indices.i[indexOffset + 0], indices.i[indexOffset + 1], indices.i[indexOffset + 2]);
	
	const vec3 barycentrics = vec3(1.0 - attrib.x - attrib.y, attrib.x, attrib.y);

	//this could have precision issue if the hit point is very far
	//vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitEXT;
	//vertices of the triangle
	vec3 v0 = vertices.v[triangleIndex.x];
	vec3 v1 = vertices.v[triangleIndex.y];
	vec3 v2 = vertices.v[triangleIndex.z];
	vec3 worldPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
	worldPos = vec3(gl_ObjectToWorldEXT * vec4(worldPos, 1.0));

	//normals of the triangle
	vec3 n0 = normals.n[triangleIndex.x];
	vec3 n1 = normals.n[triangleIndex.y];
	vec3 n2 = normals.n[triangleIndex.z];
	vec3 normal = n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z;
	normal = normalize(vec3(normal * gl_WorldToObjectEXT));

	vec2 uv0 = texcoord0s.t[triangleIndex.x];
	vec2 uv1 = texcoord0s.t[triangleIndex.y];
	vec2 uv2 = texcoord0s.t[triangleIndex.z];
	vec2 texcoord0 = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

	ShadeMaterial material = materials.m[materialIndex];
	vec3 emittance = material.emissiveFactor;
	
	vec3 textureColor = vec3(1.f);
	if(material.baseColorTextureIndex > -1)
		textureColor = texture(textures[material.baseColorTextureIndex], texcoord0).xyz;

	//new ray -> wi
	vec3 rayOrigin = worldPos;
	prd.normal = normal;

	/*
	* explicit light connection
	*/
	vec3 sphereCenter = pc.lightPos; // light position
	float sphereRadius = pc.lightRadius;
	vec3 sphereNormal = vec3(0.f);
	vec3 spherePoint = vec3(0.f);
	sampleSphere(sphereCenter, sphereRadius, rnd(prd.seed), rnd(prd.seed), sphereNormal, spherePoint);
	if(pc.shadow == 1){
		spherePoint = pc.lightPos;
	}
	
	float p = pdfLight(sphereRadius) / geometryFactor(worldPos, spherePoint, normal, sphereNormal);
	vec3 rayDirection = normalize(spherePoint - worldPos);

	float tMin = 0.001;
	float tMax = 1000000;
	uint flags = gl_RayFlagsOpaqueEXT;

	//trace ray to the light sphere
	prdDirectLightConnection = false;
	traceRayEXT(topLevelAS,
		flags | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsTerminateOnFirstHitEXT,
		0xFF,
		0,
		0,
		1,
		rayOrigin,
		0.001,
		rayDirection,
		length(spherePoint - worldPos) + 1,
		1
	);

	if(prdDirectLightConnection && p > 0) {
		vec3 f = textureColor * evalScattering(normal, rayDirection, material.baseColorFactor.xyz); // = NL * kd / PI
		prd.hitValue += 1.f * prd.weight * f / p * pc.lightIntensity;
	}

	/*
	* implicit light connection
	*/
	rayDirection = sampleBRDF(normal, rnd(prd.seed), rnd(prd.seed)); // = wi

	vec3 f = textureColor * evalScattering(normal, rayDirection, material.baseColorFactor.xyz); // = NL * kd / PI
	p = pdfBRDF(normal, rayDirection) * russianRoulette;
	prd.p = p;
	if(p < EPSILON)
		return;

	prd.weight *= f/p;

	if(length(emittance) > 0){
		prd.hitValue += 1.0f * prd.weight * emittance;
	}
	
	prd.rayOrigin = rayOrigin + rayDirection * EPSILON;
	prd.rayDirection = rayDirection;

	/*
	//not optimized 
	//recursive call
	if(prd.depth < 10){
		prd.depth++;
		float tMin = 0.001;
		float tMax = 1000000;
		uint flags = gl_RayFlagsOpaqueEXT;
		traceRayEXT(topLevelAS,
			flags,
			0xFF,
			0,
			0,
			0,
			rayOrigin,
			tMin,
			rayDirection,
			tMax,
			0);
	}
	*/
	
	//nvidia example
	/*
	const float p = 1/ PI;
	float cos_theta = max(dot(rayDirection, normal), 0);
	vec3 BRDF = material.baseColorFactor.xyz / PI;

	if(prd.depth < 10){
		prd.depth++;
		float tmin = 0.001;
		float tmax = 1000000;
		uint flags = gl_RayFlagsOpaqueEXT;
		traceRayEXT(topLevelAS,
			flags,
			0xFF,
			0,
			0,
			0,
			rayOrigin,
			tmin,
			rayDirection,
			tmax,
			0);
	}
	vec3 incoming = prd.hitValue;
	prd.hitValue = emittance + (BRDF * incoming * cos_theta / p);
	*/
}
