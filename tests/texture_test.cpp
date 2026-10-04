#include "texture.hpp"
#include "scene_io.hpp"
#include "game.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F operation,const char* message) {
    bool rejected=false; try {operation();} catch(const std::exception&) {rejected=true;}
    require(rejected,message);
}
int main() {
    auto directory=std::filesystem::temp_directory_path()/std::filesystem::path("swan-texture-test-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    try {
        auto white=swan::whiteTexture();
        require(white==swan::whiteTexture() && white->rgba==std::vector<uint8_t>({255,255,255,255}),"Fallback texture not shared/white");
        auto whiteMips=swan::buildMipChain(*white);
        require(whiteMips.size()==1 && whiteMips[0].rgba==white->rgba,"White mip chain incorrect");
        swan::TextureData contrast{2,1,{0,0,0,0,255,255,255,255}};
        auto mips=swan::buildMipChain(contrast);
        require(mips.size()==2 && mips[1].width==1 && mips[1].height==1,"Rectangular mip dimensions incorrect");
        require(mips[1].rgba==std::vector<uint8_t>({188,188,188,128}),"Mip filtering is not linear RGB/alpha");
        swan::TextureData odd{3,1,{0,0,0,255,0,0,0,255,255,255,255,255}};
        require(swan::buildMipChain(odd)[1].rgba[0]==156,"Odd edge pixel lost");
        swan::TextureData tall{1,5,std::vector<uint8_t>(20,255)};
        auto tallMips=swan::buildMipChain(tall);
        require(tallMips.size()==3 && tallMips[1].height==2 && tallMips[2].height==1 && tallMips[2].rgba[0]==255,"Tall mip chain incorrect");
        rejects([&]{swan::buildMipChain(swan::TextureData{0,1,{}});},"Zero size mip source accepted");
        rejects([&]{swan::buildMipChain(swan::TextureData{2,2,{255}});},"Truncated mip source accepted");
        auto root=std::filesystem::path(SWAN_TEST_ASSET_DIR);
        auto source=root/"textures/courtyard.png";
        auto texture=swan::loadPngTexture(source);
        require(texture->width==128 && texture->height==128 && texture->rgba.size()==128*128*4,"PNG dimensions/pixels incorrect");
        require(texture->rgba[3]==255 && texture->rgba[0]<texture->rgba[4*(16*128+16)],"PNG rows/colors/alpha decoded incorrectly");
        auto local=directory/"tiles.png"; std::filesystem::copy_file(source,local);
        swan::TextureAssets assets; assets.load("one",local); assets.load("two",local);
        require(assets.get("one").data==assets.get("two").data,"Texture aliases not shared");
        rejects([&]{assets.load("builtin:white",local);},"Built-in texture override accepted");
        auto document=swan::loadScene(root/"scenes/textured-garden.swan.json");
        auto floor=document.scene.get(document.scene.find("floor"));
        require(floor && document.scene.assets().get(floor->materialId).uvScale==glm::vec2(8),"UV scale not loaded");
        document.scene.textures().load("stone-tiles",local);
        auto scenePath=directory/"world.json"; swan::saveScene(scenePath,document);
        auto loaded=swan::loadScene(scenePath);
        require(loaded.scene.textures().get("stone-tiles").source==std::filesystem::canonical(local),"Export did not rebase texture path");
        auto text=swan::serializeScene(loaded); auto json=nlohmann::json::parse(text);
        require(json["version"]==4,"Texture export has wrong schema version");
        auto legacy=json; legacy["version"]=2; legacy.erase("textures");
        for(auto& material:legacy["materials"]) {material.erase("texture"); material.erase("uv_scale");}
        require(swan::parseScene(legacy.dump()).scene.size()==8,"Legacy version 2 mesh scene no longer loads");
        legacy["textures"]=nlohmann::json::object();
        rejects([&]{swan::parseScene(legacy.dump());},"Version 2 accepted texture fields");
        json["materials"]["floor"]["texture"]="missing";
        rejects([&]{swan::parseScene(json.dump());},"Unknown texture accepted");
        json=nlohmann::json::parse(text); json["materials"]["floor"]["uv_scale"]={0,1};
        rejects([&]{swan::parseScene(json.dump());},"Invalid UV scale accepted");
        swan::Game game(swan::gardenFromScene(std::move(loaded.scene),loaded.spawn),false,scenePath);
        auto before=game.renderFrame();
        {std::ofstream bad(local); bad << "not a PNG";}
        rejects([&]{game.reloadScene();},"Corrupt PNG reload accepted");
        require(game.renderFrame().objects.front().texture==before.objects.front().texture,"Failed reload replaced live texture");
        rejects([&]{swan::loadPngTexture(local);},"Corrupt PNG accepted");
        rejects([&]{swan::saveScene(scenePath,document);},"Export accepted missing/corrupt asset");
        std::filesystem::copy_file(source,local,std::filesystem::copy_options::overwrite_existing);
        game.reloadScene();
        require(game.renderFrame().objects.front().texture!=before.objects.front().texture,"Reload did not replace PNG resource");
        auto obj=directory/"uv.obj";
        {std::ofstream f(obj); f << "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.25 0.75\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n";}
        auto mesh=swan::loadObjMesh(obj);
        require(mesh->vertices[0].uv==glm::vec2(0.25f,0.25f),"OBJ V coordinate not converted to PNG row convention");
        {std::ofstream f(obj); f << "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/99 3/1\n";}
        rejects([&]{swan::loadObjMesh(obj);},"Invalid OBJ UV index accepted");
        {std::ofstream f(obj); f << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.5 0.5\nf 1/1/1 2/2/1 3/3/1\nf 1/5/1 3/3/1 4/4/1\n";}
        auto seam=swan::loadObjMesh(obj);
        require(seam->vertices.size()==5 && seam->indices.size()==6,"UV seam collapsed distinct texture coordinates");
        std::filesystem::remove_all(directory);
        std::cout << "PNG decode, asset sharing, UVs, persistence and texture reload passed\n";
    } catch(const std::exception& e) {std::filesystem::remove_all(directory); std::cerr << e.what() << '\n'; return 1;}
}
