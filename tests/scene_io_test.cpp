#include "scene_io.hpp"
#include "garden.hpp"
#include "game.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using Json=nlohmann::json;
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F operation,const char* message) {
    bool rejected=false;
    try { operation(); } catch(const std::exception&) { rejected=true; }
    require(rejected,message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path); return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
int main() {
    auto directory=std::filesystem::temp_directory_path()/std::filesystem::path("swan-scene-test-"+std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    try {
        auto garden=swan::makeGarden();
        auto shardKey=garden.scene.get(garden.shards.front())->key;
        auto text=swan::serializeScene({garden.scene,garden.spawn});
        auto restored=swan::parseScene(text);
        require(swan::serializeScene(restored)==text,"Scene round trip is not deterministic");
        require(restored.scene.size()==442,"Entities lost on round trip");
        require(restored.scene.assets().entries().size()<30,"Materials were not shared");
        auto keyTest=restored.scene;
        auto deleted=keyTest.entities().front(); auto deletedKey=keyTest.get(deleted)->key;
        keyTest.destroy(deleted);
        auto newId=keyTest.create({});
        require(keyTest.get(newId)->key!=deletedKey,"Generated stable key was reused after loading");
        auto loadedGarden=swan::gardenFromScene(restored.scene,restored.spawn);
        require(loadedGarden.shards.size()==5 && loadedGarden.scene.get(loadedGarden.core),"Game roles lost on round trip");
        auto json=Json::parse(text);
        auto mutated=json; mutated["version"]=2;
        rejects([&]{swan::parseScene(mutated.dump());},"Unsupported version accepted");
        mutated=json; mutated["entities"][0]["transform"]["scale"][0]=0;
        rejects([&]{swan::parseScene(mutated.dump());},"Invalid scale accepted");
        mutated=json; mutated["entities"][1]["id"]=mutated["entities"][0]["id"];
        rejects([&]{swan::parseScene(mutated.dump());},"Duplicate stable IDs accepted");
        mutated=json; mutated["entities"][0]["material"]="missing";
        rejects([&]{swan::parseScene(mutated.dump());},"Missing material accepted");
        mutated=json; mutated["entities"][0]["mesh"]="missing.obj";
        rejects([&]{swan::parseScene(mutated.dump());},"Unsupported mesh accepted");
        mutated=json; mutated["entities"][0]["solidd"]=true;
        rejects([&]{swan::parseScene(mutated.dump());},"Misspelled component accepted");
        mutated=json; mutated["spawn"]={0,0};
        rejects([&]{swan::parseScene(mutated.dump());},"Malformed spawn accepted");
        mutated=json; mutated["entities"][0]["transform"]["yaw"]=1e100;
        rejects([&]{swan::parseScene(mutated.dump());},"Out-of-range float accepted");
        mutated=json; mutated["entities"][0]["animation"]={{"base_height",0},{"phase",0},{"bob",0.2},{"speed",1}};
        rejects([&]{swan::parseScene(mutated.dump());},"Animated static collider accepted");
        rejects([&]{swan::parseScene("{broken");},"Malformed JSON accepted");
        auto path=directory/"world.json";
        swan::saveScene(path,restored);
        auto fromDisk=swan::loadScene(path);
        require(swan::serializeScene(fromDisk)==text,"Disk round trip changed scene");
        auto original=read(path);
        fromDisk.scene.get(fromDisk.scene.entities()[0])->transform.scale.x=-1;
        rejects([&]{swan::saveScene(path,fromDisk);},"Invalid runtime data saved");
        require(read(path)==original,"Failed save damaged existing file");
        auto blocked=directory/"blocked";
        std::filesystem::create_directory(blocked);
        rejects([&]{swan::saveScene(blocked,restored);},"Failed replacement of directory succeeded");
        require(std::filesystem::is_directory(blocked),"Failed save damaged destination directory");
        size_t files=0;
        for(const auto& entry:std::filesystem::directory_iterator(directory)) { (void)entry; ++files; }
        require(files==2,"Failed save leaked temporary file");
        swan::Game game(std::move(loadedGarden),false,path,directory/"saved.json");
        auto eyeBefore=game.camera().position; auto beforeCount=game.scene().size();
        { std::ofstream output(path); output << "{broken"; }
        rejects([&]{game.reloadScene();},"Invalid reload succeeded");
        swan::Input reload; reload.reload=true; game.handleInput(reload);
        require(game.status().find("Reload failed")!=std::string::npos,"Reload input did not report failure");
        require(game.scene().size()==beforeCount && game.camera().position==eyeBefore,"Failed reload changed game state");
        swan::saveScene(path,restored);
        swan::Input collect; collect.interact=true;
        auto shardPosition=game.scene().get(game.scene().find(shardKey));
        // Stable identity is preserved through file loading, independent of runtime slot handles.
        require(shardPosition && shardPosition->collectible,"Stable shard key not preserved");
        auto position=shardPosition->transform.position;
        game.spawn({position.x,0.2f,position.z+0.5f}); game.handleInput(collect);
        require(game.collected()==1,"Loaded collectible not interactive");
        game.fixedUpdate(1.0f/120,{});
        game.saveDefinition(directory/"saved.json");
        auto authored=swan::loadScene(directory/"saved.json");
        require(authored.scene.size()==442 && swan::serializeScene(authored)==text,"Export captured gameplay/animation instead of definition");
        game.handleInput(reload);
        require(game.status().find("Scene reloaded")!=std::string::npos,"Reload input did not report success");
        require(game.collected()==0 && game.scene().size()==442,"Successful reload did not reset progress");
        auto coreId=restored.scene.entities()[0]; restored.scene.get(coreId)->goal=true;
        swan::saveScene(path,restored);
        rejects([&]{game.reloadScene();},"Duplicate garden goals accepted");
        require(game.scene().size()==442 && game.collected()==0,"Semantic load failure corrupted game");
        swan::MaterialAssets materials;
        materials.set("shared",{{0.2f,0.3f,0.4f},1});
        materials.set("shared",{{0.5f,0.6f,0.7f},2});
        require(materials.get("shared").emission==2,"Material update not visible by name");
        rejects([&]{materials.set("bad",{{-1,0,0},0});},"Invalid material accepted");
        std::filesystem::remove_all(directory);
        std::cout << "Scene schema, assets, atomic save and transactional reload passed\n";
    } catch(const std::exception& e) {
        std::filesystem::remove_all(directory);
        std::cerr << e.what() << '\n'; return 1;
    }
}
