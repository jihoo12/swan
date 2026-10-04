#include "editor_view.hpp"
#include <cmath>
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
    swan::Transform t;t.position={1,2,3};t.scale={2,0.5f,4};t.yaw=0.8f;
    auto decoded=swan::transformFromMatrix(swan::transformMatrix(t));
    require(glm::length(decoded.position-t.position)<1e-5f && glm::length(decoded.scale-t.scale)<1e-5f && std::abs(decoded.yaw-t.yaw)<1e-5f,"Matrix round trip failed");
    auto corners=swan::worldBounds(scene,root);
    require(glm::length(corners[0]-corners[7])>1,"World bounds collapsed");
    swan::Transform parentWorld;parentWorld.position={3,1,-2};parentWorld.scale=glm::vec3(2);parentWorld.yaw=1.1f;
    auto childWorld=swan::composeTransform(parentWorld,t);
    auto relative=swan::relativeTransform(parentWorld,childWorld);
    require(glm::length(relative.position-t.position)<1e-4f && glm::length(relative.scale-t.scale)<1e-5f && std::abs(relative.yaw-t.yaw)<1e-5f,"relativeTransform is not the inverse of composeTransform");
    swan::Camera above;above.position={0,10,0};above.pitch=-1.2f;above.yaw=0;
    auto ground=swan::groundPoint(above,{640,400},{1280,800});
    require(ground && std::abs(ground->y)<1e-4f && ground->x>0,"Ground drop point not on the plane ahead");
    swan::Camera sky;sky.pitch=0.5f;
    require(!swan::groundPoint(sky,{640,100},{1280,800}),"Sky ray hit the ground");
    swan::MaterialAssets materials;materials.set("gold",{});materials.set("gold-2",{});
    require(swan::uniqueMaterialId(materials,"gold")=="gold-3" && swan::uniqueMaterialId(materials,"jade")=="jade","Unique material IDs");
    std::cout<<"Camera-relative creation, picking, gizmo transforms and drop placement passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
