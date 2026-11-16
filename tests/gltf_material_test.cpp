#include "editor_document.hpp"
#include "fx_json.hpp"
#include "model.hpp"
#include "scene_io.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using Json=nlohmann::json;
void require(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
template<class F> void rejects(F f,const std::string& fragment) {
    try {f();}
    catch(const std::exception& e) {
        require(std::string(e.what()).find(fragment)!=std::string::npos,"Rejected for the wrong reason: "+std::string(e.what())+" (expected '"+fragment+"')");
        return;
    }
    throw std::runtime_error("Accepted invalid input (expected '"+fragment+"')");
}
bool near(glm::vec3 a,glm::vec3 b) {return glm::length(a-b)<1e-4f;}
bool mentions(const swan::ImportedMaterial& m,const std::string& text) {
    for(const auto& note:m.unsupported) if(note.find(text)!=std::string::npos) return true;
    return false;
}
void writeJson(const std::filesystem::path& path,const Json& json) {std::ofstream out(path);out<<json.dump();}
// The four lantern parts convert identically from JSON glTF (data URIs) and GLB (buffer views).
void lantern(const std::filesystem::path& path) {
    auto model=swan::loadStaticGltf(path);
    require(model.parts.size()==4 && model.materials.size()==4,"Lantern parts/materials: "+path.string());
    const auto& bronze=*model.materials[0];
    require(bronze.name=="bronze" && near(bronze.color,{0.55f,0.33f,0.18f}) && bronze.emission==0 && !bronze.texture,"Bronze base color factor");
    require(bronze.unsupported.empty(),"Metallic factor alone should not be reported");
    const auto& painted=*model.materials[1];
    require(painted.texture && painted.texture->width==16 && painted.texture->height==16 && near(painted.color,glm::vec3(1)),"Embedded PNG base color texture");
    // Top-left texel is the cream checker square.
    require(painted.texture->rgba[0]==240 && painted.texture->rgba[1]==225 && painted.texture->rgba[2]==190,"Embedded PNG pixels");
    const auto& flame=*model.materials[2];
    require(std::abs(flame.emission-4)<1e-4f && near(flame.color,{1,0.75f,0.35f}),"Emissive strength conversion: "+std::to_string(flame.emission));
    const auto& glass=*model.materials[3];
    require(mentions(glass,"transparency") && mentions(glass,"normal texture") && !glass.texture,"Unsupported glass features are reported");
}
int main() {
    auto directory=std::filesystem::temp_directory_path()/("swan-gltf-material-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    try {
        auto assets=std::filesystem::path(SWAN_TEST_ASSET_DIR);
        lantern(assets/"models/lantern.gltf");
        lantern(assets/"models/lantern.glb");
        // Parts sharing a material share one converted entry; the old fixture has none (white).
        auto sculpture=swan::loadStaticGltf(assets/"models/sculpture.gltf");
        require(sculpture.materials.size()==2 && sculpture.materials[0]==sculpture.materials[1],"Shared default material");
        std::cout<<"lantern materials (glTF and GLB): ok\n";

        std::ifstream input(assets/"models/lantern.gltf");Json fixture;input>>fixture;
        // External PNG next to the model, resolved relative to the glTF file.
        std::filesystem::copy_file(assets/"textures/courtyard.png",directory/"albedo.png");
        auto external=fixture;external["images"][0]={{"uri","albedo.png"}};
        writeJson(directory/"external.gltf",external);
        auto courtyard=swan::loadPngTexture(assets/"textures/courtyard.png");
        auto loaded=swan::loadStaticGltf(directory/"external.gltf");
        require(loaded.materials[1]->texture && loaded.materials[1]->texture->rgba==courtyard->rgba,"External PNG texture");
        // A missing external texture fails the import, like a missing buffer.
        external["images"][0]={{"uri","missing.png"}};writeJson(directory/"missing.gltf",external);
        rejects([&]{swan::loadStaticGltf(directory/"missing.gltf");},"missing.png");
        // JPEG is reported and drawn with the color factor only.
        auto jpeg=fixture;
        jpeg["images"][0]={{"uri","data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAP//////////////////////////////////////////////////////////////////////////////////////2wBDAf//////////////////////////////////////////////////////////////////////////////////////wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAf/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFAEBAAAAAAAAAAAAAAAAAAAAAP/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AKAA/9k="}};
        writeJson(directory/"jpeg.gltf",jpeg);
        auto jpegModel=swan::loadStaticGltf(directory/"jpeg.gltf");
        require(!jpegModel.materials[1]->texture && mentions(*jpegModel.materials[1],"only PNG"),"JPEG texture reported as unsupported");
        // Corrupt PNG bytes fail clearly instead of drawing garbage.
        auto corrupt=fixture;corrupt["images"][0]={{"uri","data:image/png;base64,iVBORw0KGgoAAAAA"}};
        writeJson(directory/"corrupt.gltf",corrupt);
        rejects([&]{swan::loadStaticGltf(directory/"corrupt.gltf");},"corrupt.gltf");
        auto negative=fixture;negative["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"]={-1,0,0,1};
        writeJson(directory/"negative.gltf",negative);
        rejects([&]{swan::loadStaticGltf(directory/"negative.gltf");},"base color");
        std::cout<<"external, unsupported, and invalid textures: ok\n";

        // Scenes: builtin:imported resolves per mesh part; catalog materials still override.
        auto document=swan::loadScene(assets/"scenes/lantern-garden.swan.json");
        const auto& scene=document.scene;
        auto look=[&](const char* key){return scene.appearance(*scene.get(scene.find(key)));};
        require(near(look("left-base").material.color,{0.55f,0.33f,0.18f}),"Imported base color in scene");
        require(look("left-body").texture==scene.meshes().get("lantern-body").material->texture,"Imported texture in scene");
        require(std::abs(look("center-flame").material.emission-4)<1e-4f,"GLB imported emission in scene");
        require(near(look("right-body").material.color,scene.assets().get("jade").color) && look("right-body").texture==swan::whiteTexture(),"Catalog material override");
        // Built-in meshes have no imported material: white, untextured.
        swan::Entity cube;cube.materialId=swan::importedMaterialId;
        require(near(scene.appearance(cube).material.color,glm::vec3(1)) && scene.appearance(cube).texture==swan::whiteTexture(),"Imported material on a cube");
        // The reserved ID is never serialized as a catalog entry and survives a round trip.
        auto text=swan::serializeScene(document);
        auto json=Json::parse(text);
        require(!json["materials"].contains(swan::importedMaterialId) && json["version"]==8,"Reserved material serialized");
        auto saved=directory/"lantern.json";swan::saveScene(saved,document);
        auto again=swan::loadScene(saved);
        require(again.scene.get(again.scene.find("left-body"))->materialId==swan::importedMaterialId,"Imported material reference round trip");
        auto legacy=json;legacy["version"]=7;legacy.erase("environment");
        rejects([&]{swan::parseScene(legacy.dump());},"version 8");
        auto reserved=json;reserved["materials"]["builtin:imported"]={{"color",{1,1,1}},{"emission",0}};
        rejects([&]{swan::parseScene(reserved.dump());},"reserved");
        std::cout<<"scene builtin:imported references: ok\n";

        // Editing: entities can switch to and from the imported material with undo; the reserved
        // ID cannot be redefined or animated.
        swan::EditorDocument editor(document);
        const auto* body=editor.document().scene.get(editor.document().scene.find("right-body"));
        editor.apply(swan::SetEntityProperties{"right-body",body->name,body->meshId,swan::importedMaterialId,body->solid,body->collectible,body->goal});
        require(editor.document().scene.appearance(*editor.document().scene.get(editor.document().scene.find("right-body"))).texture!=swan::whiteTexture(),"Switch to imported material");
        require(editor.undo() && editor.document().scene.get(editor.document().scene.find("right-body"))->materialId=="jade","Undo imported material switch");
        rejects([&]{editor.apply(swan::CreateMaterial{swan::importedMaterialId,{}});},"builtin:imported");
        rejects([&]{editor.apply(swan::SetMaterial{swan::importedMaterialId,{}});},"reserved");
        auto timeline=editor.document().scene.timeline();
        swan::TimelineTrack track;track.kind=swan::TrackTarget::Material;track.target=swan::importedMaterialId;track.property=swan::TrackProperty::Color;
        track.vector.keys={{0,glm::vec3(1)},{1,glm::vec3(0)}};
        timeline.tracks.push_back(track);
        rejects([&]{editor.apply(swan::SetTimeline{timeline});},"builtin:imported");
        std::cout<<"editing and reserved-ID protection: ok\n";
        std::filesystem::remove_all(directory);
    } catch(const std::exception& e) {std::filesystem::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}
