#include "shadow.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace swan {
ShadowProjection fitSunShadow(const Camera& camera,float aspect,const Environment& environment,
                              std::span<const RenderObject> objects,uint32_t resolution) {
    if(resolution==0) throw std::invalid_argument("Shadow resolution must be positive");
    if(!(aspect>0) || !std::isfinite(aspect)) throw std::invalid_argument("Shadow aspect must be positive");
    glm::vec3 toSun=glm::normalize(environment.sunDirection);
    glm::vec3 up=std::abs(toSun.y)>0.99f?glm::vec3(0,0,1):glm::vec3(0,1,0);
    // Rotation only: snapping in light space stays stable as the camera translates.
    glm::mat4 view=glm::lookAt(glm::vec3(0),-toSun,up);
    // Bounding sphere of the camera frustum slice [near, min(far, shadowDistance)].
    float farDistance=std::clamp(environment.shadowDistance,camera.nearPlane+0.01f,std::max(camera.farPlane,camera.nearPlane+0.01f));
    float tanY=std::tan(glm::radians(camera.fov)*0.5f),tanX=tanY*aspect;
    glm::vec3 front=forward(camera.yaw,camera.pitch);
    glm::vec3 right=glm::normalize(glm::cross(front,glm::vec3(0,1,0)));
    glm::vec3 above=glm::cross(right,front);
    glm::vec3 center(0);
    glm::vec3 corners[8];
    int count=0;
    for(float d:{camera.nearPlane,farDistance})
        for(float sx:{-1.0f,1.0f}) for(float sy:{-1.0f,1.0f}) {
            corners[count]=camera.position+front*d+right*(sx*tanX*d)+above*(sy*tanY*d);
            center+=corners[count++];
        }
    center/=8.0f;
    float radius=0;
    for(const auto& corner:corners) radius=std::max(radius,glm::length(corner-center));
    // Round up so small aspect/fov jitter does not resize the texel grid every frame.
    radius=std::ceil(radius*16.0f)/16.0f;
    float texel=2*radius/float(resolution);
    glm::vec3 lightCenter=glm::vec3(view*glm::vec4(center,1));
    lightCenter.x=std::floor(lightCenter.x/texel)*texel;
    lightCenter.y=std::floor(lightCenter.y/texel)*texel;
    float left=lightCenter.x-radius,rightEdge=lightCenter.x+radius,bottom=lightCenter.y-radius,top=lightCenter.y+radius;
    // Light view looks down -z: larger z is closer to the sun.
    float nearZ=lightCenter.z+radius,farZ=lightCenter.z-radius;
    for(const auto& object:objects) {
        if(!object.mesh) continue;
        const auto& mesh=*object.mesh;
        const auto& t=object.transform;
        float c=std::cos(t.yaw),s=std::sin(t.yaw);
        glm::vec3 lo(std::numeric_limits<float>::max()),hi(std::numeric_limits<float>::lowest());
        bool finiteBounds=true;
        for(int i=0;i<8;++i) {
            glm::vec3 local((i&1)?mesh.maximum.x:mesh.minimum.x,(i&2)?mesh.maximum.y:mesh.minimum.y,(i&4)?mesh.maximum.z:mesh.minimum.z);
            local*=t.scale;
            glm::vec3 world=t.position+glm::vec3(c*local.x+s*local.z,local.y,-s*local.x+c*local.z);
            glm::vec3 p=glm::vec3(view*glm::vec4(world,1));
            if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {finiteBounds=false;break;}
            lo=glm::min(lo,p); hi=glm::max(hi,p);
        }
        if(!finiteBounds) continue;
        if(hi.x<left || lo.x>rightEdge || hi.y<bottom || lo.y>top) continue;
        nearZ=std::max(nearZ,hi.z);
    }
    // Margins keep geometry on the depth bounds from clipping.
    nearZ+=1; farZ-=1;
    // glm::ortho takes distances along -z; GLM_FORCE_DEPTH_ZERO_TO_ONE maps them to 0..1.
    glm::mat4 projection=glm::ortho(left,rightEdge,bottom,top,-nearZ,-farZ);
    return {projection*view,texel};
}
}
