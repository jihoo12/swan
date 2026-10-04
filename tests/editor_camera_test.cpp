#include "editor_camera.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
bool near(glm::vec3 a,glm::vec3 b,float epsilon=1e-3f) {return glm::length(a-b)<epsilon;}
int main() {try {
    swan::EditorCamera camera;
    camera.place({0,2,10},-1.5707963f,0);
    auto pivot=camera.pivot();
    require(near(pivot,{0,2,10-camera.pivotDistance()}),"Pivot is not straight ahead");
    camera.orbit({0.7f,0.3f});
    require(near(camera.pivot(),pivot) && std::abs(glm::length(camera.camera().position-pivot)-camera.pivotDistance())<1e-3f,"Orbit moved the pivot");
    float before=camera.pivotDistance();
    camera.dolly(2);
    require(camera.pivotDistance()<before && near(camera.pivot(),pivot),"Dolly did not zoom toward the pivot");
    for(int i=0;i<80;++i) camera.dolly(3);
    require(camera.pivotDistance()>=0.25f && std::isfinite(camera.camera().position.x),"Dolly collapsed onto the pivot");
    camera.place({0,0,0},0,0);
    camera.fly({0,0,1},false,0.5f);
    require(near(camera.camera().position,{camera.speed()*0.5f,0,0}),"Fly forward ignored speed or heading");
    camera.fly({0,1,0},true,0.5f);
    require(camera.camera().position.y>camera.speed()*1.4f,"Boosted vertical flight failed");
    camera.setSpeed(1e9f);require(camera.speed()==200,"Speed not clamped");
    camera.setSpeed(NAN);require(camera.speed()==6,"Invalid speed accepted");
    camera.place({0,0,0},0,0);
    camera.pan({100,0},800);
    require(camera.camera().position.z<0 && std::abs(camera.camera().position.y)<1e-5f,"Pan right should move the camera left");
    camera.place({0,0,0},0,0);
    camera.frame({20,0,0},1);
    require(camera.animating(),"Framing should animate");
    camera.update(0.1f);
    require(camera.camera().position.x>0 && camera.camera().position.x<20,"Framing did not interpolate");
    camera.update(1);
    require(!camera.animating() && near(camera.pivot(),{20,0,0},1e-2f),"Framing did not end on the target");
    camera.frame({0,0,0},1);camera.fly({1,0,0},false,0.1f);
    require(!camera.animating(),"User navigation must cancel framing");
    std::cout<<"Editor camera orbit, dolly, fly, pan and framing passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
