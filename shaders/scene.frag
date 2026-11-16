#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location=0) in vec3 world;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 albedo;
layout(location=3) in float glow;
layout(location=4) in vec2 uv;
layout(set=0,binding=0) uniform sampler2D baseColorTexture;
layout(set=1,binding=1) uniform sampler2DShadow shadowMap;
layout(location=0) out vec4 outColor;
// Fraction of sunlight reaching this point: 3x3 hardware-filtered PCF with a normal-offset bias,
// fading out toward the edge of the shadowed range.
float sunVisibility(vec3 n) {
    if(frame.sun.w<=0) return 1;
    vec4 clip=frame.shadowViewProjection*vec4(world+n*frame.shadow.x*1.5,1);
    vec3 p=clip.xyz/clip.w;
    vec2 uv=p.xy*0.5+0.5;
    if(any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1))) || p.z<0 || p.z>1) return 1;
    float lit=0;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) lit+=texture(shadowMap,vec3(uv+vec2(x,y)*frame.shadow.y,p.z));
    lit/=9;
    float edge=smoothstep(0.8,1.0,max(abs(p.x),abs(p.y)));
    return mix(1,mix(lit,1,edge),frame.sun.w);
}
// Linear HDR output; the post pass applies exposure, bloom, and tone mapping.
void main() {
    vec3 surface=albedo*texture(baseColorTexture,uv).rgb;
    vec3 n=normalize(normal);
    float sun=max(dot(n,frame.sun.xyz),0);
    if(sun>0) sun*=sunVisibility(n);
    vec3 light=surface*(frame.light.x+sun*frame.light.y);
    vec3 delta=vec3(0,3.2,0)-world;
    light+=surface*vec3(0.3,0.9,1.4)*max(dot(n,normalize(delta)),0)*8.0/(1+dot(delta,delta))*frame.light.z;
    light+=surface*glow;
    light=mix(light,frame.fog.rgb,fogAmount(length(world-frame.eye.xyz)));
    outColor=vec4(light,1);
}
