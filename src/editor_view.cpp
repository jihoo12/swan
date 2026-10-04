#include "editor_view.hpp"
#include <glm/gtc/matrix_transform.hpp>
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
glm::mat4 transformMatrix(const Transform& t) {
    auto matrix=glm::translate(glm::mat4(1),t.position);
    matrix=glm::rotate(matrix,t.yaw,glm::vec3(0,1,0));
    return glm::scale(matrix,t.scale);
}
Transform transformFromMatrix(const glm::mat4& m) {
    Transform t;
    t.position=glm::vec3(m[3]);
    t.scale={glm::length(glm::vec3(m[0])),glm::length(glm::vec3(m[1])),glm::length(glm::vec3(m[2]))};
    // Column 2 of rotateY(yaw) is (sin, 0, cos).
    t.yaw=std::atan2(m[2].x,m[2].z);
    return t;
}
std::array<glm::vec3,8> worldBounds(const Scene& scene,EntityId id) {
    const auto& mesh=*scene.meshes().get(scene.get(id)->meshId).data;
    auto matrix=transformMatrix(scene.worldTransform(id));
    std::array<glm::vec3,8> corners;
    for(int i=0;i<8;++i) {
        glm::vec3 local{i&1?mesh.maximum.x:mesh.minimum.x,i&2?mesh.maximum.y:mesh.minimum.y,i&4?mesh.maximum.z:mesh.minimum.z};
        corners[i]=glm::vec3(matrix*glm::vec4(local,1));
    }
    return corners;
}
std::optional<glm::vec3> groundPoint(const Camera& camera,glm::vec2 pixel,glm::vec2 display,float maxDistance) {
    if(display.x<=0 || display.y<=0) return std::nullopt;
    auto inverse=glm::inverse(camera.viewProjection(display.x/display.y));
    auto unproject=[&](float depth) {auto p=inverse*glm::vec4(pixel/display*2.0f-1.0f,depth,1);return glm::vec3(p)/p.w;};
    auto origin=unproject(0),direction=glm::normalize(unproject(1)-origin);
    if(std::abs(direction.y)<1e-5f) return std::nullopt;
    float distance=-origin.y/direction.y;
    if(distance<=0 || distance>maxDistance) return std::nullopt;
    return origin+direction*distance;
}
std::string uniqueMaterialId(const MaterialAssets& materials,const std::string& base) {
    if(!materials.contains(base)) return base;
    for(int i=2;;++i) if(auto id=base+"-"+std::to_string(i);!materials.contains(id)) return id;
}
}
