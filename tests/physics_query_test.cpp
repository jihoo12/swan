#include "editor_document.hpp"
#include "physics.hpp"
#include "scene_io.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
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
swan::EntityId box(swan::Scene& scene,std::string key,glm::vec3 position,glm::vec3 scale,float yaw=0,bool solid=true,bool trigger=false) {
    swan::Entity e;e.key=std::move(key);e.transform={position,scale,yaw};e.solid=solid;e.trigger=trigger;
    return scene.create(e);
}
int main() {
    try {
        swan::Scene scene;
        auto floor=box(scene,"floor",{0,-0.5f,0},{20,1,20});
        auto wall=box(scene,"wall",{0,1,-5},{4,2,1});
        auto prop=box(scene,"prop",{3,0.5f,0},{1,1,1},0,false);
        auto zone=box(scene,"zone",{0,1,-2},{2,2,2},0,false,true);
        // Nearest hit, point, normal, and distance.
        auto down=swan::raycast(scene,{0,5,0},{0,-2,0},100);
        require(down && down->entity==floor && near(down->point,{0,0,0}) && near(down->normal,{0,1,0}) && std::abs(down->distance-5)<1e-4f,"Downward ray to the floor");
        auto ahead=swan::raycast(scene,{0,1,0},{0,0,-1},100);
        require(ahead && ahead->entity==wall && std::abs(ahead->distance-4.5f)<1e-4f && near(ahead->normal,{0,0,1}),"Forward ray skips the trigger and hits the wall");
        auto withTriggers=swan::raycast(scene,{0,1,0},{0,0,-1},100,{.triggers=true});
        require(withTriggers && withTriggers->entity==zone && std::abs(withTriggers->distance-1)<1e-4f,"Triggers are hit only when requested");
        require(!swan::raycast(scene,{0,1,0},{0,0,-1},4),"Max distance is respected");
        require(!swan::raycast(scene,{0,1,0},{0,1,0},100),"Ray into empty sky");
        // Origin inside a box: that box is skipped (here the floor), the next one is found.
        auto inside=swan::raycast(scene,{0,-0.5f,0},{0,0,-1},100);
        require(!inside,"Ray from inside the floor should not hit the floor");
        // Rotated boxes: the hit normal follows the box yaw.
        swan::Scene turned;
        box(turned,"slab",{0,0,0},{2,2,2},0.7853982f);
        auto diagonal=swan::raycast(turned,{5,0,0},{-1,0,0},100);
        require(diagonal && std::abs(diagonal->distance-(5-std::sqrt(2.0f)))<1e-3f,"Yaw-rotated box distance");
        require(std::abs(diagonal->normal.y)<1e-5f && std::abs(glm::length(diagonal->normal)-1)<1e-5f && std::abs(std::abs(diagonal->normal.x)-std::sqrt(0.5f))<1e-4f,"Yaw-rotated box normal");
        // Filters.
        require(swan::raycast(scene,{3,5,0},{0,-1,0},100)->entity==prop,"Non-solid props are hit by default");
        require(swan::raycast(scene,{3,5,0},{0,-1,0},100,{.solidOnly=true})->entity==floor,"solid_only skips props");
        require(swan::raycast(scene,{3,5,0},{0,-1,0},100,{.ignore=prop})->entity==floor,"ignore skips one entity");
        rejects([&]{swan::raycast(scene,{0,0,0},{0,0,0},1);},"nonzero");
        rejects([&]{swan::raycast(scene,{0,0,0},{0,1,0},-1);},"distance");
        rejects([&]{swan::raycast(scene,{0,NAN,0},{0,1,0},1);},"finite");
        // Overlaps: boxes touching the sphere, in scene order; triggers only on request.
        auto near3=swan::overlapSphere(scene,{3,1.2f,0},0.5f);
        require(near3.size()==1 && near3[0]==prop,"Sphere overlap of the prop");
        auto all=swan::overlapSphere(scene,{0,1,-2},0.5f,{.triggers=true});
        require(all.size()==1 && all[0]==zone,"Sphere overlap inside a trigger");
        require(swan::overlapSphere(scene,{0,1,-2},0.5f).empty(),"Triggers hidden from overlaps by default");
        rejects([&]{swan::overlapSphere(scene,{0,0,0},-1);},"radius");
        // Player capsule against a trigger.
        require(swan::capsuleTouches(scene,zone,{0,0,-2}) && !swan::capsuleTouches(scene,zone,{0,0,2}),"Capsule trigger contact");
        // Triggers never block movement.
        auto walk=swan::moveCapsule(scene,{0,0.0f,0},{0,0,-3});
        require(std::abs(walk.feet.z+3)<1e-3f,"Trigger blocked the capsule");
        std::cout<<"raycasts, overlaps, and capsule contacts: ok\n";

        // Triggers persist in version 8 and are validated.
        swan::Entity bad;bad.key="bad";bad.trigger=true;bad.solid=true;
        rejects([&]{scene.create(bad);},"trigger");
        swan::SceneDocument document{scene,{0,0,0}};
        auto json=nlohmann::json::parse(swan::serializeScene(document));
        auto zoneJson=std::find_if(json["entities"].begin(),json["entities"].end(),[](const auto& e){return e["id"]=="zone";});
        require((*zoneJson)["trigger"]==true,"Trigger serialized");
        auto floorJson=std::find_if(json["entities"].begin(),json["entities"].end(),[](const auto& e){return e["id"]=="floor";});
        require(!floorJson->contains("trigger"),"Non-triggers omit the field");
        auto loaded=swan::parseScene(json.dump());
        require(loaded.scene.get(loaded.scene.find("zone"))->trigger,"Trigger round trip");
        auto legacy=json;legacy["version"]=7;
        rejects([&]{swan::parseScene(legacy.dump());},"version 8");
        (*zoneJson)["trigger"]="yes";
        rejects([&]{swan::parseScene(json.dump());},"boolean");
        // Editing keeps the trigger flag unless the command sets it; making it solid is rejected.
        swan::EditorDocument editor(loaded);
        editor.apply(swan::SetEntityProperties{"zone","Renamed","builtin:cube","default"});
        require(editor.document().scene.get(editor.document().scene.find("zone"))->trigger,"Rename cleared the trigger flag");
        rejects([&]{editor.apply(swan::SetEntityProperties{"zone","Zone","builtin:cube","default",true});},"trigger");
        editor.apply(swan::SetEntityProperties{"zone","Zone","builtin:cube","default",false,false,false,false});
        require(!editor.document().scene.get(editor.document().scene.find("zone"))->trigger,"Clear trigger flag");
        require(editor.undo() && editor.document().scene.get(editor.document().scene.find("zone"))->trigger,"Undo trigger change");
        std::cout<<"trigger persistence and editing: ok\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
