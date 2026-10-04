#include "camera_rig.hpp"
#include "game.hpp"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
bool close(glm::vec3 a,glm::vec3 b) {return glm::length(a-b)<0.001f;}
int main() {try {
    swan::Scene scene;swan::Camera aim;aim.yaw=-1.5707963268f;aim.pitch=0;
    swan::ThirdPersonRig rig;glm::vec3 target{0,1.65f,0};
    require(close(rig.camera(scene,aim,target).position,{0,1.65f,4}),"Unobstructed follow distance incorrect");
    swan::Entity wall;wall.solid=true;wall.transform={{0,2,2},{5,4,0.2f},0};auto w=scene.create(wall);
    auto clipped=rig.camera(scene,aim,target);
    require(clipped.position.z>1.6f && clipped.position.z<1.7f,"Camera crossed thin wall");
    scene.get(w)->solid=false;require(close(rig.camera(scene,aim,target).position,{0,1.65f,4}),"Nonsolid object blocked camera");
    scene.get(w)->solid=true;scene.get(w)->transform.position=target;
    require(close(rig.camera(scene,aim,target).position,target),"Starting inside collider did not collapse boom");
    scene.get(w)->transform={{0,2,2},{5,4,0.2f},0.78539816f};
    require(rig.camera(scene,aim,target).position.z<2,"Rotated wall ignored");
    swan::Entity root;root.transform.position={0,0,10};auto r=scene.create(root);scene.setParent(w,r);
    require(close(rig.camera(scene,aim,target).position,{0,1.65f,4}),"World-space parent transform ignored");
    swan::Game first,third;third.setThirdPerson(true);auto count=first.scene().size();
    require(third.renderFrame().objects.size()==count+1 && third.scene().size()==count,"Avatar modified authored scene");
    swan::Input move;move.move.y=1;
    for(int i=0;i<60;++i) {first.fixedUpdate(1.0f/120,move);third.fixedUpdate(1.0f/120,move);}
    require(close(first.renderFrame().camera.position,third.renderFrame().objects.back().transform.position+glm::vec3(0,0.75f,0)),"Third-person mode changed controller motion");
    swan::Input toggle;toggle.toggleCamera=true;third.handleInput(toggle);
    require(!third.isThirdPerson() && close(first.camera().position,third.camera().position),"Camera toggle teleported player");
    third.handleInput(toggle);third.reloadScene();require(third.isThirdPerson(),"Reload lost camera preference");
    swan::Input flight;flight.toggleFlight=true;third.handleInput(flight);
    require(third.flying() && third.renderFrame().objects.size()==count,"Flight retained follow avatar");
    std::cout<<"Camera boom collision, hierarchy and third-person controller passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
