#include "editor_view.hpp"
#include <cmath>
namespace swan {
std::string pickEntity(const Scene& scene,const Camera& camera,glm::vec2 pixel,glm::vec2 display) {
    if(display.x<=0 || display.y<=0 || !std::isfinite(pixel.x) || !std::isfinite(pixel.y)) return {};
    auto inverse=glm::inverse(camera.viewProjection(display.x/display.y));
    auto unproject=[&](float depth) {
        auto point=inverse*glm::vec4(pixel/display*2.0f-1.0f,depth,1);
        return glm::vec3(point)/point.w;
    };
    auto origin=unproject(0),segment=unproject(1)-origin;
    float nearest=1;std::string selected;
    for(auto id:scene.entities()) {
        const auto& entity=*scene.get(id);auto world=scene.worldTransform(id);
        float c=std::cos(world.yaw),s=std::sin(world.yaw);
        auto local=[&](glm::vec3 p) {return glm::vec3(c*p.x-s*p.z,p.y,s*p.x+c*p.z)/world.scale;};
        auto ray=local(segment),start=local(origin-world.position);
        const auto& mesh=*scene.meshes().get(entity.meshId).data;
        float enter=0,exit=nearest;bool hit=true;
        for(int axis=0;axis<3;++axis) {
            if(std::abs(ray[axis])<1e-8f) {
                if(start[axis]<mesh.minimum[axis] || start[axis]>mesh.maximum[axis]) {hit=false;break;}
            } else {
                float a=(mesh.minimum[axis]-start[axis])/ray[axis],b=(mesh.maximum[axis]-start[axis])/ray[axis];
                if(a>b) std::swap(a,b);
                enter=std::max(enter,a);exit=std::min(exit,b);
                if(enter>exit) {hit=false;break;}
            }
        }
        if(!hit) continue;
        for(size_t i=0;i+2<mesh.indices.size();i+=3) {
            auto a=mesh.vertices[mesh.indices[i]].position;
            auto edge1=mesh.vertices[mesh.indices[i+1]].position-a,edge2=mesh.vertices[mesh.indices[i+2]].position-a;
            auto cross=glm::cross(ray,edge2);float determinant=glm::dot(edge1,cross);
            if(std::abs(determinant)<1e-8f) continue;
            auto offset=start-a;float u=glm::dot(offset,cross)/determinant;
            if(u<0 || u>1) continue;
            auto q=glm::cross(offset,edge1);float v=glm::dot(ray,q)/determinant;
            if(v<0 || u+v>1) continue;
            float distance=glm::dot(edge2,q)/determinant;
            if(distance>=0 && distance<nearest) {nearest=distance;selected=entity.key;}
        }
    }
    return selected;
}
Entity editorCube(const Camera& camera) {
    Entity cube;cube.name="Cube";cube.transform.position=camera.position+forward(camera.yaw,camera.pitch)*5.0f;
    return cube;
}
}
