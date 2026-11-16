#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
// Depth-only sun shadow pass: same object transform as scene.vert, projected from the sun.
layout(location=0) in vec3 vertexPosition;
layout(push_constant) uniform Push {
    vec4 positionUvV;   // xyz position.
    vec4 scaleGlow;     // xyz scale.
    vec4 color;         // a: yaw.
    vec4 extra;
} p;
void main() {
    float angle=p.color.a;
    mat3 rotation=mat3(cos(angle),0,-sin(angle),0,1,0,sin(angle),0,cos(angle));
    vec3 world=rotation*(vertexPosition*p.scaleGlow.xyz)+p.positionUvV.xyz;
    gl_Position=frame.shadowViewProjection*vec4(world,1);
}
