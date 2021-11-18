const float PI = 3.141592;

//rotation between v1 and v2
vec4 quaternionfFromTwoVectors(vec3 v1, vec3 v2){
	vec4 q;
	q.xyz = cross(v1, v2);
	q.w = sqrt(dot(v1, v1) * dot(v2, v2)) + dot(v1,v2);
	q = normalize(q);
	return q;
}

//quaternion vector multiplication
vec3 qtransform(vec4 q, vec3 v) {
	vec3 t = 2 * cross(q.xyz, v);
	return v + q.w * t + cross(q.xyz, t);//2.0 * cross((cross(v, q.xyz) + q.w*v), q.wyz);
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

void sampleSphere(vec3 center, float radius, float randomFloat1, float randomFloat2, out vec3 normal, out vec3 point){
	float z = 2.f * randomFloat1 - 1.f;
	float r = sqrt(1 - z*z);
	float a = 2 * PI * randomFloat2;
	normal = normalize(vec3(r * cos(a), r * sin(a), z));
	point = center + radius * normal;
}

float geometryFactor(vec3 intersectionPointA, vec3 intersectionPointB, vec3 normalA, vec3 normalB){
	vec3 D = intersectionPointA - intersectionPointB;
	return abs(dot(normalA, D) * dot(normalB, D) / pow(dot(D, D), 2));
}

float pdfLight(float sphereRadius){
	return 1 / (4 * PI * sphereRadius * sphereRadius); //reciprocal of surface area of sphere
}
