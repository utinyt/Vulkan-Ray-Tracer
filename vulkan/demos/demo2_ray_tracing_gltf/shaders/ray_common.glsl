#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
/*
* struct definition - thie file is to be included
*/
struct hitPayload{
	vec3 hitValue;
	uint seed;
	uint depth;
	vec3 weight;
	vec3 rayOrigin;
	vec3 rayDirection;
};

struct RtPushConstant{
	vec4 clearColor;
	vec3 lightPos;
	float lightRadius;
	float lightIntensity;
	int64_t frame;
	int maxDepht;
	int rayPerPixel;
};
