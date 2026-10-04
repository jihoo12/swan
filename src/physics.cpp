#include "physics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace swan {
namespace {
glm::vec3 rotate(glm::vec3 p,float yaw) {
    float c=std::cos(yaw),s=std::sin(yaw);
    return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
}
struct Contact { glm::vec3 normal{}; float depth=0; };
Contact capsuleBox(glm::vec3 feet,float radius,float height,const Transform& box) {
    glm::vec3 p=rotate(feet-box.position,-box.yaw),half=box.scale*0.5f;
    float low=p.y+radius,high=p.y+height-radius;
    glm::vec3 separation{p.x-std::clamp(p.x,-half.x,half.x),0,p.z-std::clamp(p.z,-half.z,half.z)};
    if(low>half.y) separation.y=low-half.y;
    else if(high<-half.y) separation.y=high+half.y;
    float distance=glm::length(separation);
    if(distance>=radius) return {};
    if(distance>0.000001f) return {rotate(separation/distance,box.yaw),radius-distance};
    // The capsule axis intersects the box. Pick the shortest exit translation.
    glm::vec3 exits[6]={{half.x+radius-p.x,0,0},{-half.x-radius-p.x,0,0},
        {0,half.y+radius-low,0},{0,-half.y-radius-high,0},
        {0,0,half.z+radius-p.z},{0,0,-half.z-radius-p.z}};
    auto best=exits[0];
    for(auto exit:exits) if(glm::length(exit)<glm::length(best)) best=exit;
    float depth=glm::length(best);
    return depth>0?Contact{rotate(best/depth,box.yaw),depth}:Contact{};
}
}
MotionResult moveCapsule(const Scene& scene,glm::vec3 feet,glm::vec3 displacement,float radius,float height) {
    if(!std::isfinite(radius) || !std::isfinite(height) || radius<=0 || height<2*radius)
        throw std::invalid_argument("Invalid capsule dimensions");
    for(int i=0;i<3;++i) if(!std::isfinite(feet[i]) || !std::isfinite(displacement[i]))
        throw std::invalid_argument("Nonfinite capsule movement");
    float distance=glm::length(displacement);
    if(distance>100) throw std::invalid_argument("Capsule displacement exceeds safe movement limit");
    int steps=std::max(1,int(std::ceil(distance/(radius*0.5f))));
    glm::vec3 step=displacement/float(steps);
    auto ids=scene.entities();
    MotionResult result{feet};
    for(int i=0;i<steps;++i) {
        result.feet+=step;
        for(int iteration=0;iteration<8;++iteration) {
            bool corrected=false;
            for(auto id:ids) {
                const auto* entity=scene.get(id);
                if(!entity->solid) continue;
                const auto& mesh=*scene.meshes().get(entity->meshId).data;
                Transform proxy=scene.worldTransform(id);
                auto center=(mesh.minimum+mesh.maximum)*0.5f;
                proxy.position+=rotate(center*proxy.scale,proxy.yaw);
                proxy.scale*=(mesh.maximum-mesh.minimum);
                auto contact=capsuleBox(result.feet,radius,height,proxy);
                if(contact.depth<=0) continue;
                result.feet+=contact.normal*(contact.depth+0.00001f);
                if(contact.normal.y>0.5f) result.grounded=true;
                if(contact.normal.y<-0.5f) result.ceiling=true;
                corrected=true;
            }
            if(!corrected) break;
        }
    }
    return result;
}
}
