const float PI = 3.141592;

//rotation between v1 and v2
vec4 quaternionfFromTwoVectors(vec3 v1, vec3 v2){
	vec4 q;
	q.xyz = cross(v1, v2);
	float v1Len = length(v1);
	float v2Len = length(v2);
	q.w = sqrt(v1Len * v1Len * v2Len * v2Len) + dot(v1,v2);
	q = normalize(q);
	return q;
}

//quaternion vector multiplication
vec3 qtransform(vec4 q, vec3 v) {
	return v + 2.0 * cross((cross(v, q.xyz) + q.w*v), q.wyz);
}

//importance sampling
vec3 sampleLobe(vec3 normal, float c, float phi) {
	float s = sqrt(1 - c*c);
	vec3 k = vec3(s * cos(phi), s * sin(phi), c);
	vec4 q = quaternionfFromTwoVectors(vec3(0, 0, 1), normal);
	return normalize(qtransform(q, k));
}

float pdfBRDF(vec3 normal, vec3 wi) {
	return abs(dot(normal, wi)) / PI;
}

vec3 sampleBRDF(vec3 normal, float randomFloat1, float randomFloat2){
	return sampleLobe(normal, sqrt(randomFloat1), 2 * PI * randomFloat2);
}

vec3 evalScattering(vec3 normal, vec3 wi, vec3 kd){
	return abs(dot(normal, wi)) * kd / PI;
}
