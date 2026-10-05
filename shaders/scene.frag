#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location=0) in vec3 world;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 albedo;
layout(location=3) in float glow;
layout(location=4) in vec2 uv;
layout(set=0,binding=0) uniform sampler2D baseColorTexture;
layout(location=0) out vec4 outColor;
// Linear HDR output; the post pass applies exposure, bloom, and tone mapping.
void main() {
    vec3 surface=albedo*texture(baseColorTexture,uv).rgb;
    vec3 n=normalize(normal);
    float sun=max(dot(n,normalize(vec3(-0.6,1,0.35))),0);
    vec3 light=surface*(frame.light.x+sun*frame.light.y);
    vec3 delta=vec3(0,3.2,0)-world;
    light+=surface*vec3(0.3,0.9,1.4)*max(dot(n,normalize(delta)),0)*8.0/(1+dot(delta,delta))*frame.light.z;
    light+=surface*glow;
    light=mix(light,frame.fog.rgb,fogAmount(length(world-frame.eye.xyz)));
    outColor=vec4(light,1);
}
