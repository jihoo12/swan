#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in vec2 vertexUV;
layout(push_constant) uniform Push {
    vec4 positionUvV;   // xyz position, w: V tiling.
    vec4 scaleGlow;     // xyz scale, w: emission.
    vec4 color;         // rgb color, a: yaw.
    vec4 extra;         // x: U tiling.
} p;
layout(location=0) out vec3 world;
layout(location=1) out vec3 normal;
layout(location=2) out vec3 albedo;
layout(location=3) out float glow;
layout(location=4) out vec2 uv;
void main() {
    float angle=p.color.a;
    mat3 rotation=mat3(cos(angle),0,-sin(angle),0,1,0,sin(angle),0,cos(angle));
    world=rotation*(vertexPosition*p.scaleGlow.xyz)+p.positionUvV.xyz;
    normal=normalize(rotation*(vertexNormal/p.scaleGlow.xyz));
    albedo=p.color.rgb;
    glow=p.scaleGlow.w;
    uv=vertexUV*vec2(p.extra.x,p.positionUvV.w);
    gl_Position=frame.viewProjection*vec4(world,1);
}
