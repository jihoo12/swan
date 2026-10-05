#version 450
#extension GL_GOOGLE_include_directive : require
#include "frame.glsl"
layout(location=0) in vec2 uv;
layout(location=1) in vec4 color;
layout(location=2) flat in uvec2 spriteSeed;
layout(location=3) in float eyeDistance;
layout(set=0,binding=0) uniform sampler2D spriteTexture;
layout(location=0) out vec4 outColor;
layout(constant_id=0) const bool additive=true;
float hash(vec2 q) { return fract(sin(dot(q,vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 q) {
    vec2 i=floor(q),f=fract(q);
    vec2 u=f*f*(3-2*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),u.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),u.x),u.y);
}
float fbm(vec2 q) {
    float sum=0,amplitude=0.5;
    for(int i=0;i<4;++i) {sum+=noise(q)*amplitude;q=q*2.03+vec2(17.1,9.7);amplitude*=0.5;}
    return sum;
}
// Sprite IDs match swan::ParticleSprite.
float mask(uint sprite,vec2 q,float r) {
    float edge=max(fwidth(r),1e-3);
    if(sprite==1u) return 1-smoothstep(1-edge*1.5,1,r);                                    // circle
    if(sprite==2u) return smoothstep(0.6,0.75,r)*(1-smoothstep(0.88,1,r));                  // ring
    if(sprite==3u) return 1;                                                                // square
    if(sprite==4u) return clamp(exp(-r*r*12)+0.35*exp(-r*r*3),0,1)*(1-smoothstep(0.85,1,r)); // spark
    if(sprite==5u) {                                                                        // smoke
        vec2 offset=vec2(float(spriteSeed.y&1023u),float((spriteSeed.y>>10)&1023u))*0.37;
        float cloud=fbm(q*1.8+offset);
        return clamp((1-smoothstep(0.35,1,r))*(0.35+cloud*1.1),0,1);
    }
    return exp(-r*r*3.5)*(1-smoothstep(0.75,1,r));                                         // soft
}
void main() {
    vec2 q=uv*2-1;
    float r=length(q);
    vec4 texel=texture(spriteTexture,uv);
    float alpha=clamp(color.a*mask(spriteSeed.x,q,r)*texel.a,0,1);
    vec3 light=color.rgb*texel.rgb;
    // Linear HDR, premultiplied. Additive light fades into fog; alpha-blended matter takes the
    // fog color, like scene surfaces.
    float fog=fogAmount(eyeDistance);
    light=additive?light*(1-fog):mix(light,frame.fog.rgb,fog);
    outColor=vec4(light*alpha,alpha);
}
