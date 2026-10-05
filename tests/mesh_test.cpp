#include "mesh.hpp"
#include "assets.hpp"
#include "scene_io.hpp"
#include "game.hpp"
#include "physics.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F operation,const char* message) {
    bool rejected=false;
    try { operation(); } catch(const std::exception&) { rejected=true; }
    require(rejected,message);
}
void write(const std::filesystem::path& path,const std::string& text) { std::ofstream output(path); output << text; }
int main() {
    auto directory=std::filesystem::temp_directory_path()/std::filesystem::path("swan-mesh-test-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    try {
        auto cube=swan::cubeMesh();
        require(cube==swan::cubeMesh() && cube->vertices.size()==24 && cube->indices.size()==36,"Cube asset is not shared/indexed");
        require(cube->minimum==glm::vec3(-0.5f) && cube->maximum==glm::vec3(0.5f),"Cube bounds changed");
        auto fixture=std::filesystem::path(SWAN_TEST_ASSET_DIR)/"meshes/crystal.obj";
        auto crystal=swan::loadObjMesh(fixture);
        require(crystal->indices.size()==24 && crystal->minimum.y==-0.85f && crystal->maximum.y==0.85f,"Crystal OBJ/bounds incorrect");
        for(const auto& vertex:crystal->vertices) require(std::abs(glm::length(vertex.normal)-1)<0.0001f,"Generated normal not normalized");
        auto path=directory/"model.obj";
        const std::string triangle="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        write(path,triangle);
        auto flat=swan::loadObjMesh(path);
        require(flat->vertices[0].normal==glm::vec3(0,0,1),"Missing normals not generated from face winding");
        write(path,"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvn 0 0 2\nf 1//1 2//1 3//1\nf -4//1 -2//1 -1//1\n");
        auto smooth=swan::loadObjMesh(path);
        require(smooth->vertices.size()==4 && smooth->indices.size()==6,"Supplied normals/negative indices do not share vertices");
        require(smooth->vertices[0].normal==glm::vec3(0,0,1),"Supplied normals not normalized");
        write(path,"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
        rejects([&]{swan::loadObjMesh(path);},"Untriangulated face accepted");
        write(path,"v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n");
        rejects([&]{swan::loadObjMesh(path);},"Degenerate face accepted");
        write(path,"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n");
        rejects([&]{swan::loadObjMesh(path);},"Out-of-range index accepted");
        write(path,"v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 0\nf 1//1 2//1 3//1\n");
        rejects([&]{swan::loadObjMesh(path);},"Zero normal accepted");
        write(path,"v 0 0 0\n");
        rejects([&]{swan::loadObjMesh(path);},"Empty triangle mesh accepted");
        write(path,triangle);
        swan::MeshAssets meshes; meshes.load("first",path); meshes.load("second",path);
        require(meshes.get("first").data==meshes.get("second").data,"Catalog did not deduplicate mesh paths");
        rejects([&]{meshes.load("builtin:cube",path);},"Built-in mesh override accepted");
        std::filesystem::copy_file(fixture,path,std::filesystem::copy_options::overwrite_existing);
        auto garden=swan::makeGarden(); garden.scene.meshes().load("custom",path);
        garden.scene.get(garden.core)->meshId="custom";
        size_t coreIndex=0;
        for(auto id:garden.scene.entities()) { if(garden.scene.get(id)->goal) break; ++coreIndex; }
        std::filesystem::create_directory(directory/"scenes");
        auto scenePath=directory/"scenes/world.json";
        swan::saveScene(scenePath,{garden.scene,garden.spawn});
        auto document=swan::loadScene(scenePath);
        require(document.scene.meshes().get("custom").source==std::filesystem::canonical(path),"Export failed to rebase relative mesh path");
        auto json=nlohmann::json::parse(std::ifstream(scenePath));
        require(json["version"]==7 && json["meshes"]["custom"]["source"]=="../model.obj","Mesh source not persisted as relative reference");
        auto layer=swan::gardenFromScene(std::move(document.scene),document.spawn);
        swan::Game game(std::move(layer),false,scenePath);
        auto before=game.renderFrame();
        write(path,"invalid OBJ");
        rejects([&]{game.reloadScene();},"Invalid OBJ reload succeeded");
        auto after=game.renderFrame();
        require(after.objects.size()==before.objects.size() && after.objects[coreIndex].mesh==before.objects[coreIndex].mesh,"Failed mesh reload changed render resources");
        std::filesystem::copy_file(fixture,path,std::filesystem::copy_options::overwrite_existing);
        game.reloadScene();
        require(game.renderFrame().objects[coreIndex].mesh!=before.objects[coreIndex].mesh,"Reload did not replace imported mesh resource");
        write(path,"v 10 0 0\nv 10 0 2\nv 12 0 0\nf 1 2 3\n");
        swan::Scene physics; physics.meshes().load("offset",path);
        swan::Entity floor; floor.meshId="offset"; floor.solid=true; physics.create(floor);
        auto motion=swan::moveCapsule(physics,{10.5f,3,0.5f},{0,-5,0});
        require(motion.grounded && std::abs(motion.feet.y)<0.001f,"Collision proxy ignored mesh bounds/center");
        std::filesystem::remove_all(directory);
        std::cout << "OBJ import, normals, shared assets, relative paths, reload and bounds passed\n";
    } catch(const std::exception& e) {
        std::filesystem::remove_all(directory); std::cerr << e.what() << '\n'; return 1;
    }
}
