#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(push_constant) uniform Constants{
    vec4 clearColor;
};

void main()
{
    hitValue = clearColor.xyz * 0.8;
}