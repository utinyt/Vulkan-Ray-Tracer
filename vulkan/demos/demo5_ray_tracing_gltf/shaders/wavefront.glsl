/*
* struct definition - this file is to be included
*/

struct Vertex{
	vec3 pos;
	vec3 normal;
};

struct CameraMatrices{
    mat4 view;
    mat4 proj;
    mat4 viewInverse;
    mat4 projInverse;
};

struct SceneDesc{
	uint64_t vertexAddress;
	uint64_t normalAddress;
	uint64_t uvAddress;
	uint64_t indexAddress;
	uint64_t materialAddress;
	uint64_t primInfoAddress;
};

struct RasterPushConstant{
	mat4 modelMatrix;
	vec3 lightPos;
	uint objIndex;
	float lightIntensity;
	int lightType;
	int materialId;
};

struct GltfShadeMaterial{
	vec4 pbrBaseColorFactor;
	vec3 emiisiveColor;
	int pbrBaseColorTexture;
};

vec3 calculateDiffuse(GltfShadeMaterial mat, vec3 fragToLight, vec3 normal){
	fragToLight = normalize(fragToLight);
	fragToLight = normalize(fragToLight);
	float nl = max(dot(fragToLight, normal), 0);
	return mat.pbrBaseColorFactor.xyz * nl;
}

vec3 calculateSpecular(vec3 viewFragPos, vec3 fragToLight, vec3 normal){
	fragToLight = normalize(fragToLight);
	vec3 fragToView = normalize(-viewFragPos);
	vec3 reflectDir = reflect(-fragToLight, normal);
	float spec = 5 * pow(max(dot(fragToView , reflectDir), 0.0), 64);
	return vec3(spec);
}
