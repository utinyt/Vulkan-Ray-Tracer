/*
* struct definition - this file is to be included
*/

struct Vertex{
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec3 color;
	vec4 tangent;
};

struct SceneDesc{
	mat4 transform;
	mat4 transformIT;
	uint64_t vertexAddress;
	uint64_t indexAddress;
	uint64_t normalAddress;
	uint64_t uvAddress;
	uint64_t colorAddress;
	uint64_t tangentAddress;
	uint64_t materialIndicesAddress;
	uint64_t materialAddress;
	uint64_t primitiveAddress;
	uint64_t padding;
};

struct Primitive {
	uint firstIndex;
	uint indexCount;
	uint vertexStart;
	uint vertexCount;
	//int materialIndex;
};
