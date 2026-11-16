#include "editor_document.hpp"
#include "physics.hpp"
#include "scene_io.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F f,const std::string& fragment) {
    try {f();}
    catch(const std::exception& e) {
        require(std::string(e.what()).find(fragment)!=std::string::npos,"Rejected for the wrong reason: "+std::string(e.what())+" (expected '"+fragment+"')");
        return;
    }
    throw std::runtime_error("Accepted invalid input (expected '"+fragment+"')");
}
bool near(glm::vec3 a,glm::vec3 b,float tolerance=1e-4f) {return glm::length(a-b)<tolerance;}
float lineDistance(const swan::Ray& ray,glm::vec3 point) {return glm::length(glm::cross(point-ray.origin,ray.direction));}
void pixelRays() {
    auto camera=swan::lookAt({4,3,8},{0,1,0},55);
    glm::vec2 size{640,360};
    // The image center looks at the target.
    auto center=camera.pixelRay(size*0.5f,size);
    require(lineDistance(center,{0,1,0})<1e-3f && glm::dot(center.direction,glm::vec3(0,1,0)-camera.position)>0,"Center pixel misses the target");
    // Round trip: project world points to pixels with the render matrix, then cast back.
    auto matrix=camera.viewProjection(size.x/size.y);
    for(glm::vec3 p:{glm::vec3(2,0,0),glm::vec3(-1.5f,2.5f,1),glm::vec3(0.3f,0,-3)}) {
        auto clip=matrix*glm::vec4(p,1);
        glm::vec2 pixel=(glm::vec2(clip)/clip.w*0.5f+0.5f)*size;
        require(lineDistance(camera.pixelRay(pixel,size),p)<2e-3f,"Pixel ray does not pass through the projected point");
    }
    // Top rows look up: Vulkan clip space has -1 at the top of the image.
    require(camera.pixelRay({320,0},size).direction.y>camera.pixelRay({320,360},size).direction.y,"Pixel rows are flipped");
}
swan::Entity box(std::string key,glm::vec3 position,glm::vec3 scale,float yaw=0) {
    swan::Entity e;e.key=std::move(key);e.name=e.key;e.transform={position,scale,yaw};return e;
}
void boundsAndDuplicate() {
    swan::SceneDocument document;
    auto& scene=document.scene;
    auto tower=scene.create(box("tower",{0,1,0},{2,2,2},0.7853982f));
    auto top=scene.create(box("top",{0,1.5f,0},{0.5f,0.5f,0.5f}));scene.setParent(top,tower);
    auto flag=scene.create(box("flag",{0.5f,0,0},{1,1,1}));scene.setParent(flag,top);
    // A 2 m cube turned 45 degrees spans sqrt(2) along world X and Z.
    auto b=swan::worldBounds(scene,tower);
    require(near(b.minimum,{-std::sqrt(2.0f),0,-std::sqrt(2.0f)},1e-3f) && near(b.maximum,{std::sqrt(2.0f),2,std::sqrt(2.0f)},1e-3f),"Rotated bounds");
    auto all=swan::worldBounds(scene,tower,true);
    auto flagBox=swan::worldBounds(scene,flag);
    require(all.maximum.y>=flagBox.maximum.y && all.maximum.y>b.maximum.y,"Bounds with descendants");
    swan::Timeline timeline;
    swan::TimelineTrack track;track.target="tower";track.property=swan::TrackProperty::Yaw;track.scalar.keys={{0,0},{1,1}};
    timeline.tracks.push_back(track);scene.timeline()=timeline;
    swan::EditorDocument editor(document);
    auto before=editor.document().scene.size();
    editor.apply(swan::DuplicateEntity{"tower","tower-2",{5,0,0}});
    const auto& edited=editor.document().scene;
    require(edited.size()==before+3 && editor.selection()=="tower-2","Duplicate copies the subtree and selects the copy");
    auto copy=edited.find("tower-2");
    require(near(edited.worldTransform(copy).position,{5,1,0}) && std::abs(edited.worldTransform(copy).yaw-0.7853982f)<1e-6f,"Duplicate offset");
    std::vector<swan::EntityId> children;
    for(auto id:edited.entities()) if(edited.parent(id)==copy) children.push_back(id);
    require(children.size()==1,"Copied child");
    auto copiedTop=children[0];
    require(near(edited.worldTransform(copiedTop).position,edited.worldTransform(edited.find("top")).position+glm::vec3(5,0,0)),"Child keeps its place relative to the copy");
    bool grandchild=false;
    for(auto id:edited.entities()) grandchild|=edited.parent(id)==copiedTop;
    require(grandchild,"Grandchild copied");
    require(edited.timeline().tracks.size()==1,"Timeline tracks are not duplicated");
    // A copy of a child stays under the same parent.
    editor.apply(swan::DuplicateEntity{"top","",{0,1,0}});
    auto childCopy=edited.find(editor.selection());
    require(edited.parent(childCopy)==edited.find("tower") && near(edited.worldTransform(childCopy).position,edited.worldTransform(edited.find("top")).position+glm::vec3(0,1,0)),"Child duplicate keeps its parent");
    rejects([&]{editor.apply(swan::DuplicateEntity{"tower","tower-2"});},"tower-2");
    rejects([&]{editor.apply(swan::DuplicateEntity{"missing"});},"missing");
    rejects([&]{editor.apply(swan::DuplicateEntity{"tower","",{NAN,0,0}});},"finite");
    require(editor.undo() && editor.undo() && editor.document().scene.size()==before,"Undo duplicates");
    require(editor.redo() && editor.document().scene.size()==before+3,"Redo duplicate");
    auto saved=swan::parseScene(swan::serializeScene(editor.document()));
    require(saved.scene.size()==before+3 && !saved.scene.parent(saved.scene.find("tower-2")),"Saved duplicate");
}
void assets() {
    auto directory=std::filesystem::temp_directory_path()/("swan-authoring-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    auto root=std::filesystem::path(SWAN_TEST_ASSET_DIR);
    swan::EditorDocument editor(swan::SceneDocument{});
    editor.apply(swan::AddTexture{"tiles",root/"textures/courtyard.png"});
    editor.apply(swan::AddMesh{"lantern-body",root/"models/lantern.glb",1});
    editor.apply(swan::AddMesh{"crystal",root/"meshes/crystal.obj"});
    const auto& scene=editor.document().scene;
    require(scene.textures().contains("tiles") && scene.meshes().get("lantern-body").material && scene.meshes().get("lantern-body").part==1,"Assets registered");
    require(editor.undoLabel()=="Add mesh crystal","History label");
    rejects([&]{editor.apply(swan::AddTexture{"missing",directory/"missing.png"});},"missing.png");
    rejects([&]{editor.apply(swan::AddMesh{"bad",root/"models/lantern.glb",9});},"part");
    rejects([&]{editor.apply(swan::AddTexture{"builtin:x",root/"textures/courtyard.png"});},"builtin:x");
    require(!editor.document().scene.textures().contains("missing") && !editor.document().scene.meshes().contains("bad"),"Failed asset commands changed the document");
    // Registered assets are saved with paths relative to the scene file.
    swan::Material tiled;tiled.textureId="tiles";
    editor.apply(swan::CreateMaterial{"tiled",tiled});
    auto entity=box("lamp",{0,0,0},{1,1,1});entity.meshId="lantern-body";entity.materialId=swan::importedMaterialId;
    editor.apply(swan::CreateEntity{entity,std::nullopt});
    auto path=directory/"scene.swan.json";editor.save(path);
    auto loaded=swan::loadScene(path);
    require(loaded.scene.meshes().get("lantern-body").source==std::filesystem::canonical(root/"models/lantern.glb") && loaded.scene.textures().contains("tiles"),"Registered assets persisted");
    require(editor.undo() && editor.undo() && editor.undo() && !editor.document().scene.meshes().contains("crystal"),"Undo asset registration");
    std::filesystem::remove_all(directory);
}
int main() {
    try {
        pixelRays();std::cout<<"pixel rays: ok\n";
        boundsAndDuplicate();std::cout<<"bounds and duplicate: ok\n";
        assets();std::cout<<"texture and mesh registration: ok\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
