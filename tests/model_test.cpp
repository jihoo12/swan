#include "model.hpp"
#include "scene_io.hpp"
#include "game.hpp"
#include "physics.hpp"
#include "camera_rig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid glTF accepted");}
int main() {
    auto directory=std::filesystem::temp_directory_path()/std::filesystem::path("swan-model-test-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    try {
        auto assets=std::filesystem::path(SWAN_TEST_ASSET_DIR),path=assets/"models/sculpture.gltf";
        auto binary=swan::loadStaticGltf(assets/"models/sculpture.glb");
        auto model=swan::loadStaticGltf(path);
        require(binary.parts.size()==model.parts.size() && binary.parts[1]->indices==model.parts[1]->indices && binary.parts[0]->vertices[0].position==model.parts[0]->vertices[0].position,"GLB import differs from glTF");require(model.parts.size()==2,"Node instances lost");
        const auto& left=*model.parts[0];const auto& right=*model.parts[1];
        require(left.vertices.size()==24 && left.indices.size()==24,"glTF geometry lost");
        require(glm::length(left.vertices[0].position-glm::vec3(-1.5f,1.41421356f,1.41421356f))<0.001f,"Nested quaternion/nonuniform transform not baked");
        require(glm::length(left.vertices[0].uv-glm::vec2(.25f,.75f))<0.001f,"glTF UV convention incorrect");
        for(const auto* mesh:{&left,&right}) for(size_t i=0;i<mesh->indices.size();i+=3) {
            auto a=mesh->vertices[mesh->indices[i]],b=mesh->vertices[mesh->indices[i+1]],c=mesh->vertices[mesh->indices[i+2]];
            require(glm::dot(glm::normalize(glm::cross(b.position-a.position,c.position-a.position)),a.normal)>0.999f,"Inverse transpose normal or mirrored winding incorrect");
        }
        swan::MeshAssets catalog;catalog.load("one",path,0);catalog.load("alias",path,0);catalog.load("two",path,1);
        require(catalog.get("one").data==catalog.get("alias").data && catalog.get("one").data!=catalog.get("two").data,"Model sharing/part selection incorrect");
        rejects([&]{catalog.load("missing",path,2);});
        auto document=swan::loadScene(assets/"scenes/gltf-garden.swan.json");
        // The bundled sculptures are decorative. Collision is controlled by solid,
        // and the same baked glTF bounds must stop both controller and camera.
        require(!document.scene.get(document.scene.find("left-pillar"))->solid && !document.scene.get(document.scene.find("right-pillar"))->solid,"Sample collision configuration changed");
        auto collision=document.scene;
        glm::vec3 feet{-1.5f,0,-0.2f},displacement{0,0,-3};
        auto pass=swan::moveCapsule(collision,feet,displacement);
        require(std::abs(pass.feet.z+3.2f)<0.001f,"Decorative glTF blocked controller");
        auto collider=collision.find("left-pillar");collision.get(collider)->solid=true;
        auto blocked=swan::moveCapsule(collision,feet,displacement);
        require(blocked.feet.z>-0.3f,"Solid glTF bounds did not stop controller");
        swan::Camera aim;aim.yaw=1.5707963268f;aim.pitch=0;
        swan::ThirdPersonRig rig;
        auto decorative=rig.camera(document.scene,aim,{-1.5f,1.65f,0});
        auto obstructed=rig.camera(collision,aim,{-1.5f,1.65f,0});
        require(decorative.position.z<-3.9f && obstructed.position.z>-0.4f,"Solid glTF bounds did not stop camera");
        auto savedCollision=swan::parseScene(swan::serializeScene({collision,document.spawn}));
        require(savedCollision.scene.get(savedCollision.scene.find("left-pillar"))->solid,"glTF collider flag lost on persistence");
        auto output=directory/"scene.json";swan::saveScene(output,document);auto loaded=swan::loadScene(output);
        require(loaded.scene.meshes().get("gltf-right").part==1 && loaded.scene.meshes().get("gltf-left").source==std::filesystem::canonical(path),"Part/path round trip failed");
        auto json=nlohmann::json::parse(swan::serializeScene(loaded));json["meshes"]["gltf-left"]["part"]=-1;rejects([&]{swan::parseScene(json.dump());});
        json["meshes"]["gltf-left"]["part"]=0;json["version"]=4;rejects([&]{swan::parseScene(json.dump());});
        auto local=directory/"model.gltf";std::filesystem::copy_file(path,local);
        document.scene.meshes().load("gltf-left",local,0);swan::saveScene(output,document);
        swan::Game game(swan::gardenFromScene(document.scene,document.spawn),false,output);
        auto before=game.renderFrame();{std::ofstream bad(local);bad<<"broken";}
        rejects([&]{game.reloadScene();});require(game.renderFrame().objects[3].mesh==before.objects[3].mesh,"Failed model reload replaced live world");
        std::filesystem::copy_file(path,local,std::filesystem::copy_options::overwrite_existing);game.reloadScene();
        require(game.renderFrame().objects[3].mesh!=before.objects[3].mesh,"Successful reload retained old model");
        std::ifstream input(path);nlohmann::json fixture;input>>fixture;
        auto missing=fixture;missing["buffers"][0]["uri"]="missing.bin";{std::ofstream bad(local);bad<<missing.dump();}
        rejects([&]{swan::loadStaticGltf(local);});
        fixture["nodes"][1]["scale"]={0,1,1};{std::ofstream bad(local);bad<<fixture.dump();}
        rejects([&]{swan::loadStaticGltf(local);});
        std::filesystem::remove_all(directory);std::cout<<"Static glTF transforms, normals, sharing, persistence and reload passed\n";
    } catch(const std::exception& e) {std::filesystem::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}
