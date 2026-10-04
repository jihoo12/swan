#version 450
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(push_constant) uniform Push {
    mat4 viewProjection;
    vec4 positionTime;
    vec4 scaleGlow;
    vec4 color;
    vec4 eye;
} p;
layout(location=0) out vec3 world;
layout(location=1) out vec3 normal;
layout(location=2) out vec3 albedo;
layout(location=3) out float glow;
void main() {
    float angle=p.color.a;
    mat3 rotation=mat3(cos(angle),0,-sin(angle),0,1,0,sin(angle),0,cos(angle));
    world=rotation*(vertexPosition*p.scaleGlow.xyz)+p.positionTime.xyz;
    normal=normalize(rotation*(vertexNormal/p.scaleGlow.xyz));
    albedo=p.color.rgb;
    glow=p.scaleGlow.w;
    gl_Position=p.viewProjection*vec4(world,1);
}
