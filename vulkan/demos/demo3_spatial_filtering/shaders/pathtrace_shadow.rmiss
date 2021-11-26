#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "ray_common.glsl"

layout(location = 1) rayPayloadInEXT bool prdDirectLightConnection;

void main() {
    prdDirectLightConnection = true;
}