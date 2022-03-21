#version 450
//#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 outUV;
layout(location = 0) out vec4 col;

layout(set = 0, binding = 0) uniform sampler2D denoised;
layout(set = 0, binding = 1) uniform sampler2D rawDirect;
layout(set = 0, binding = 2) uniform sampler2D rawIndirect;
layout(set = 0, binding = 3) uniform sampler2D gbufferAlbedo;

layout(push_constant) uniform PushConstant{
	uint denoise;
};

void main(){
	if(denoise != 0)
		col = texture(denoised, outUV);
	else{
		vec4 albedo = texture(gbufferAlbedo, outUV);
		vec4 direct = texture(rawDirect, outUV);
		vec4 indirect = texture(rawIndirect, outUV);
		col = (direct + indirect) / 2.f;
		col.xyz *= albedo.xyz;
	}
}
