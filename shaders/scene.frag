#version 450
layout(push_constant) uniform Push {
    mat4 viewProjection; vec4 positionUvV; vec4 scaleGlow; vec4 color; vec4 eyeUvU;
} p;
layout(location=0) in vec3 world;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 albedo;
layout(location=3) in float glow;
layout(location=4) in vec2 uv;
layout(set=0,binding=0) uniform sampler2D baseColorTexture;
layout(location=0) out vec4 outColor;
void main() {
    vec3 surface=albedo*texture(baseColorTexture,uv).rgb;
    vec3 n=normalize(normal);
    float sun=max(dot(n,normalize(vec3(-0.6,1,0.35))),0);
    vec3 light=surface*(0.24+sun*0.9);
    vec3 delta=vec3(0,3.2,0)-world;
    light+=surface*vec3(0.3,0.9,1.4)*max(dot(n,normalize(delta)),0)*8.0/(1+dot(delta,delta));
    light+=surface*glow;
    float distanceToEye=length(world-p.eyeUvU.xyz);
    float fog=1-exp(-distanceToEye*distanceToEye*0.00065);
    vec3 fogColor=vec3(0.035,0.065,0.095);
    light=mix(light,fogColor,fog);
    outColor=vec4(light/(1+light),1);
}
