#include "camera_rig.hpp"
#include <stdexcept>
namespace swan {
namespace {
glm::vec3 rotate(glm::vec3 p,float yaw) {
    float c=std::cos(yaw),s=std::sin(yaw);
    return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
}
}
Camera ThirdPersonRig::camera(const Scene& scene,const Camera& aim,glm::vec3 target) const {
    if(!std::isfinite(distance) || distance<=0 || !std::isfinite(clearance) || clearance<=0)
        throw std::invalid_argument("Invalid third-person rig distance/clearance");
    auto boom=-forward(aim.yaw,aim.pitch)*distance;
    for(int i=0;i<3;++i) if(!std::isfinite(target[i]) || !std::isfinite(boom[i])) throw std::invalid_argument("Invalid camera target/orientation");
    float fraction=1;
    for(auto id:scene.entities()) {
        auto* entity=scene.get(id);if(!entity->solid) continue;
        auto world=scene.worldTransform(id);
        const auto& mesh=*scene.meshes().get(entity->meshId).data;
        // Expand the yaw-aligned box for a conservative camera sphere sweep.
        auto minimum=mesh.minimum*world.scale-glm::vec3(clearance);
        auto maximum=mesh.maximum*world.scale+glm::vec3(clearance);
        auto origin=rotate(target-world.position,-world.yaw),direction=rotate(boom,-world.yaw);
        float enter=0,exit=1;bool hit=true;
        for(int axis=0;axis<3;++axis) {
            if(std::abs(direction[axis])<1e-7f) {
                if(origin[axis]<minimum[axis] || origin[axis]>maximum[axis]) {hit=false;break;}
            } else {
                float a=(minimum[axis]-origin[axis])/direction[axis],b=(maximum[axis]-origin[axis])/direction[axis];
                if(a>b) std::swap(a,b);
                enter=std::max(enter,a);exit=std::min(exit,b);
                if(enter>exit) {hit=false;break;}
            }
        }
        if(hit) fraction=std::min(fraction,std::max(0.0f,enter-0.01f/distance));
    }
    Camera result=aim;result.position=target+boom*fraction;return result;
}
}
