#include "editor_view.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {try {
    swan::Camera camera;camera.position={0,0,8};camera.yaw=-1.5707963268f;camera.pitch=0;
    swan::Scene scene;auto cube=swan::editorCube(camera);cube.key="front";auto front=scene.create(cube);
    require(glm::length(cube.transform.position-glm::vec3(0,0,3))<1e-5f,"Cube not in front of camera");
    auto back=cube;back.key="back";back.transform.position.z=0;scene.create(back);
    require(swan::pickEntity(scene,camera,{640,400},{1280,800})=="front","Nearest surface not selected");
    require(swan::pickEntity(scene,camera,{640,10},{1280,800}).empty(),"Empty space selected an entity");
    require(swan::pickEntity(scene,camera,{320,200},{640,400})=="front","Resize changed picking");
    require(swan::pickEntity(scene,camera,{0,0},{0,0}).empty(),"Zero-size viewport accepted");
    scene.get(front)->transform.position={0,0,10};
    require(swan::pickEntity(scene,camera,{640,400},{1280,800})=="back","Behind-camera object selected");
    swan::Entity parent;parent.key="parent";parent.transform.position={2,0,0};parent.transform.yaw=1.5707963268f;
    auto root=scene.create(parent);scene.setParent(front,root);
    scene.get(front)->transform.position={-3,0,-2};scene.get(front)->transform.scale={2,1,.5f};
    require(swan::pickEntity(scene,camera,{640,400},{1280,800})=="front","Transformed child picking failed");
    std::cout<<"Camera-relative creation and nearest mesh picking passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
