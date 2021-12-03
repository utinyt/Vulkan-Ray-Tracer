#version 450
//#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 col;

layout(binding = 0) uniform sampler2D color;
layout(binding = 1) uniform sampler2D pos;
layout(binding = 2) uniform sampler2D normal;

layout(push_constant) uniform PuchConstant {
	float stepSize;
	float cphi;
	float pphi;
	float nphi;
};

const float kernel[25] = float[25](
	1.0 / 256.0,	4.0 / 256.0,	6.0 / 256.0,	4.0 / 256.0,	1.0 / 256.0, //16
	4.0 / 256.0,	16.0 / 256.0,	24.0 / 256.0,	16.0 / 256.0,	4.0 / 256.0, //64
	6.0 / 256.0,	24.0 / 256.0,	36.0 / 256.0,	24.0 / 256.0,	6.0 / 256.0, //96
	4.0 / 256.0,	16.0 / 256.0,	24.0 / 256.0,	16.0 / 256.0,	4.0 / 256.0, //64
	1.0 / 256.0,	4.0 / 256.0,	6.0 / 256.0,	4.0 / 256.0,	1.0 / 256.0 //16
);

const vec2 offsets[25] = vec2[25](
	vec2(-2, -2),	vec2(-1, -2),	vec2(0, -2),	vec2(1, -2),	vec2(2, -2),
	vec2(-2, -1),	vec2(-1, -1),	vec2(0, -1),	vec2(1, -1),	vec2(2, -1),
	vec2(-2, 0),	vec2(-1, 0),	vec2(0, 0),		vec2(1, 0),		vec2(2, 0),
	vec2(-2, 1),	vec2(-1, 1),	vec2(0, 1),		vec2(1, 1),		vec2(2, 1),
	vec2(-2, 2),	vec2(-1, 2),	vec2(0, 2),		vec2(1, 2),		vec2(2, 2)
);

void main(){
	vec2 step = vec2(1.0) / textureSize(color, 0).xy;
	vec4 p_color	= texture(color, inUV);
	vec4 p_pos		= texture(pos, inUV);
	vec4 p_normal	= texture(normal, inUV);

	vec4 sum = vec4(0.0);			//sum of hi(q) * w(p, q) * ci(p)
	float normalizationFactor = 0;	//k

	//stepSize [0, 4]
	float stepScale = 0;
	for(int i = 0; i < stepSize; ++i){
		stepScale += 1 << i;
	}

	//float stepScale = 1 << int(stepSize);
	//float colorScale = pow(2, -stepScale);

	for(int i = 0; i < 25; ++i){
		vec2 uv = inUV + offsets[i] * step * stepScale;

		vec4 q_color = texture(color, uv);
		vec4 color_diff = p_color - q_color;
		float dist2 = dot(color_diff, color_diff);
		float color_weight = min(exp(-(dist2)/(cphi)), 1.0);

		vec4 q_normal = texture(normal, uv);
		vec4 normal_diff = p_normal - q_normal; 
		dist2 = max(dot(normal_diff, normal_diff) / (stepScale*stepScale), 0.0);
		float normal_weight = min(exp(-(dist2)/nphi), 1.0);

		vec4 q_pos = texture(pos, uv);
		vec4 pos_diff = p_pos - q_pos; 
		dist2 = dot(pos_diff, pos_diff);
		float pos_weight = min(exp(-(dist2)/pphi), 1.0);

		float weight = color_weight * pos_weight * normal_weight;
		sum += q_color * weight * kernel[i];
		normalizationFactor += weight * kernel[i];
	}
	col = sum / normalizationFactor;
}
