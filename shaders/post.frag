#version 450
// Bloom (threshold + downsample chain + tent upsample) and the final exposure/tone-map composite.
layout(location=0) in vec2 uv;
layout(set=0,binding=0) uniform sampler2D source;
layout(set=0,binding=1) uniform sampler2D bloom;
layout(push_constant) uniform Push {
    vec4 texel;    // xy: 1 / source size.
    vec4 params;   // x: threshold, y: bloom strength, z: exposure, w: 1 / bloom levels.
    ivec4 mode;    // x: 0 prefilter, 1 downsample, 2 upsample, 3 composite; y: tone map (0 Reinhard, 1 ACES).
} p;
layout(location=0) out vec4 outColor;
layout(constant_id=0) const bool encodeSrgb=false;
vec3 srgb(vec3 c) { return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c)); }
vec3 fetch(vec2 at) { return min(texture(source,at).rgb,vec3(256)); } // Clamp fireflies/inf.
// 13-tap downsample (Jimenez 2014): stable under motion, no aliasing of thin sparks.
vec3 downsample() {
    vec2 t=p.texel.xy;
    vec3 a=fetch(uv+t*vec2(-2,-2)),b=fetch(uv+t*vec2(0,-2)),c=fetch(uv+t*vec2(2,-2));
    vec3 d=fetch(uv+t*vec2(-1,-1)),e=fetch(uv+t*vec2(1,-1));
    vec3 f=fetch(uv+t*vec2(-2,0)),g=fetch(uv),h=fetch(uv+t*vec2(2,0));
    vec3 i=fetch(uv+t*vec2(-1,1)),j=fetch(uv+t*vec2(1,1));
    vec3 k=fetch(uv+t*vec2(-2,2)),l=fetch(uv+t*vec2(0,2)),m=fetch(uv+t*vec2(2,2));
    return (d+e+i+j)*0.125+(a+b+f+g)*0.03125+(b+c+g+h)*0.03125+(f+g+k+l)*0.03125+(g+h+l+m)*0.03125;
}
vec3 upsample() {
    vec2 t=p.texel.xy;
    vec3 sum=fetch(uv)*4;
    sum+=(fetch(uv+vec2(-t.x,0))+fetch(uv+vec2(t.x,0))+fetch(uv+vec2(0,-t.y))+fetch(uv+vec2(0,t.y)))*2;
    sum+=fetch(uv+vec2(-t.x,-t.y))+fetch(uv+vec2(t.x,-t.y))+fetch(uv+vec2(-t.x,t.y))+fetch(uv+vec2(t.x,t.y));
    return sum/16;
}
vec3 aces(vec3 x) { return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0,1); }
void main() {
    if(p.mode.x==0) {
        vec3 c=downsample();
        float brightness=max(c.r,max(c.g,c.b));
        float knee=max(p.params.x*0.5,1e-4);
        float soft=clamp(brightness-p.params.x+knee,0,2*knee);
        soft=soft*soft/(4*knee);
        c*=max(soft,brightness-p.params.x)/max(brightness,1e-4);
        outColor=vec4(c,1);
    } else if(p.mode.x==1) outColor=vec4(downsample(),1);
    else if(p.mode.x==2) outColor=vec4(upsample(),1);
    else {
        vec3 color=texture(source,uv).rgb;
        // The upsample chain sums every level; normalize so strength is independent of resolution.
        if(p.params.y>0) color+=texture(bloom,uv).rgb*(p.params.y*p.params.w);
        color*=p.params.z;
        vec3 mapped=p.mode.y==1?aces(color):color/(1+color);
        outColor=vec4(encodeSrgb?srgb(mapped):mapped,1);
    }
}
