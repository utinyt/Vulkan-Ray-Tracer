#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "ray_common.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(push_constant) uniform PushConstant {
	RtPushConstant pc;
};

void main() {
    if(prd.depth == 0){
        prd.direct = pc.clearColor.xyz * 0.8;
        prd.indirect = pc.clearColor.xyz * 0.8;
    }
    else{
        prd.direct = vec3(0.01);
        prd.indirect = vec3(0.01);
    }
    prd.depth = 100;
}