#include "visibility.hpp"
#include "camera.hpp"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
int main() {try {
    swan::Camera camera;camera.position={0,0,0};camera.yaw=-1.5707963268f;camera.pitch=0;camera.fov=90;camera.nearPlane=1;camera.farPlane=10;
    auto matrix=camera.viewProjection(1);
    swan::Frustum frustum(matrix);
    auto cube=swan::cubeMesh();
    swan::Transform box;box.position={0,0,-5};
    require(frustum.intersects(*cube,box),"Visible cube culled");
    for(auto p:{glm::vec3(0,0,3),glm::vec3(0,0,-12),glm::vec3(0,0,-0.2f),glm::vec3(8,0,-5),glm::vec3(-8,0,-5),glm::vec3(0,8,-5),glm::vec3(0,-8,-5)}) {
        box.position=p;require(!frustum.intersects(*cube,box),"Outside cube not culled");
    }
    box.position={0,0,-1};require(frustum.intersects(*cube,box),"Near intersection culled");
    box.position={0,0,-10};require(frustum.intersects(*cube,box),"Far intersection culled");
    box.position={0,0,0};box.scale={30,30,30};require(frustum.intersects(*cube,box),"Camera-enclosing box culled");
    box.position={7,0,-5};box.scale={8,1,1};box.yaw=0.4f;require(frustum.intersects(*cube,box),"Rotated nonuniform edge box culled");
    swan::MeshData offset;offset.minimum={9,-1,-6};offset.maximum={11,1,-4};box={};box.position={-10,0,0};
    require(frustum.intersects(offset,box),"Off-center mesh bounds ignored");
    box.position={0,0,-5};box.scale={1,1,1};
    swan::Frustum wide(camera.viewProjection(2));box.position={7,0,-5};
    require(!frustum.intersects(*cube,box) && wide.intersects(*cube,box),"Aspect ratio not applied");
    offset.minimum.x=std::numeric_limits<float>::quiet_NaN();require(frustum.intersects(offset,box),"Invalid bounds hidden");
    // Independent oracle: an interior point in clip volume means its box must survive.
    std::mt19937 random(42);std::uniform_real_distribution<float> position(-15,15),scale(0.1f,8),angle(-3.14f,3.14f);
    int witnesses=0;
    for(int i=0;i<1000;++i) {
        box={{position(random),position(random),position(random)},{scale(random),scale(random),scale(random)},angle(random)};
        float c=std::cos(box.yaw),s=std::sin(box.yaw);
        for(float x:{-0.5f,0.0f,0.5f}) for(float y:{-0.5f,0.0f,0.5f}) for(float z:{-0.5f,0.0f,0.5f}) {
            auto p=glm::vec3(x,y,z)*box.scale;
            auto clip=matrix*glm::vec4(box.position+glm::vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z),1);
            if(clip.w>0 && std::abs(clip.x)<clip.w && std::abs(clip.y)<clip.w && clip.z>0 && clip.z<clip.w) {
                ++witnesses;require(frustum.intersects(*cube,box),"Visible sampled point culled");
            }
        }
    }
    require(witnesses>100,"Random test did not sample visible geometry");
    std::cout<<"Frustum boundaries, transformed bounds and visibility witnesses passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
