#version 450
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
const vec3 normals[6] = vec3[](vec3(0,0,1),vec3(0,0,-1),vec3(1,0,0),vec3(-1,0,0),vec3(0,1,0),vec3(0,-1,0));
const vec3 tangents[6] = vec3[](vec3(1,0,0),vec3(-1,0,0),vec3(0,0,-1),vec3(0,0,1),vec3(1,0,0),vec3(1,0,0));
const vec2 corners[6] = vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
void main() {
    int face=gl_VertexIndex/6;
    vec3 n=normals[face], t=tangents[face], b=cross(n,t);
    vec2 uv=corners[gl_VertexIndex%6];
    vec3 local=(n+t*uv.x+b*uv.y)*0.5*p.scaleGlow.xyz;
    float angle=p.color.a;
    mat3 rotation=mat3(cos(angle),0,-sin(angle),0,1,0,sin(angle),0,cos(angle));
    world=rotation*local+p.positionTime.xyz;
    normal=rotation*n;
    albedo=p.color.rgb;
    glow=p.scaleGlow.w;
    gl_Position=p.viewProjection*vec4(world,1);
}
