#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
// One camera-facing quad per instance (ParticleVertex); six vertices, no index buffer.
layout(location=0) in vec3 particlePosition;
layout(location=1) in float particleSize;
layout(location=2) in vec4 particleColor;
layout(location=3) in vec3 particleVelocity;
layout(location=4) in float particleRotation;
layout(location=5) in float particleStretch;
layout(location=6) in uvec2 particleSpriteSeed;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 color;
layout(location=2) flat out uvec2 spriteSeed;
layout(location=3) out float eyeDistance;
const vec2 corners[6]=vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
void main() {
    vec2 corner=corners[gl_VertexIndex];
    float halfSize=particleSize*0.5;
    vec3 right=frame.right.xyz,up=frame.up.xyz;
    vec2 screenVelocity=vec2(dot(particleVelocity,right),dot(particleVelocity,up));
    float speed=length(screenVelocity);
    vec3 world;
    if(particleStretch>0 && speed>1e-4) {
        // Long axis along the on-screen velocity, lengthened with speed (sparks, streaks, rain).
        vec2 d=screenVelocity/speed;
        vec3 along=right*d.x+up*d.y,across=up*d.x-right*d.y;
        world=particlePosition+along*corner.x*halfSize*(1+particleStretch*speed)+across*corner.y*halfSize;
    } else {
        float c=cos(particleRotation),s=sin(particleRotation);
        vec2 r=vec2(c*corner.x-s*corner.y,s*corner.x+c*corner.y);
        world=particlePosition+(right*r.x+up*r.y)*halfSize;
    }
    uv=corner*0.5+0.5;
    color=particleColor;
    spriteSeed=particleSpriteSeed;
    eyeDistance=length(world-frame.eye.xyz);
    gl_Position=frame.viewProjection*vec4(world,1);
}
