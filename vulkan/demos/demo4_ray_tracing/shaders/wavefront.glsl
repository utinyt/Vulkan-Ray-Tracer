/*
* struct definition - this file is to be included
*/

struct Vertex{
	vec3 pos;
	vec3 normal;
};

struct SceneDesc{
	mat4 transform;
	mat4 transformIT;
	uint64_t vertexAddress;
	uint64_t indexAddress;
};
