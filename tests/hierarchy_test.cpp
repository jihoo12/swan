#include "scene_io.hpp"
#include "physics.hpp"
#include "game.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid hierarchy accepted");}
bool close(glm::vec3 a,glm::vec3 b) {return glm::length(a-b)<0.0001f;}
int main() {try {
    swan::Scene scene;
    swan::Entity root;root.key="root";root.goal=true;root.transform={{10,0,0},{2,2,2},1.5707963268f};
    auto r=scene.create(root);
    swan::Entity child;child.key="child";child.collectible=true;child.transform.position={1,0,0};
    auto c=scene.create(child);scene.setParent(c,r);
    auto world=scene.worldTransform(c);
    require(close(world.position,{10,0,-2}) && close(world.scale,{2,2,2}),"Composition incorrect");
    rejects([&]{scene.setParent(r,c);});require(!scene.parent(r),"Cycle failure mutated tree");
    auto json=nlohmann::json::parse(swan::serializeScene({scene,{}}));
    std::swap(json["entities"][0],json["entities"][1]);
    auto loaded=swan::parseScene(json.dump());
    require(close(loaded.scene.worldTransform(loaded.scene.find("child")).position,world.position),"Forward parent reference failed");
    auto bad=json;bad["entities"][0]["parent"]="missing";rejects([&]{swan::parseScene(bad.dump());});
    bad=json;bad["version"]=3;rejects([&]{swan::parseScene(bad.dump());});
    bad=json;bad["entities"][1]["parent"]="child";rejects([&]{swan::parseScene(bad.dump());});
    swan::Game game(swan::gardenFromScene(scene,{}),false);
    require(close(game.renderFrame().objects[1].transform.position,world.position),"Renderer ignored hierarchy");
    auto position=world.position;
    game.spawn(position-glm::vec3(0,1.65f,0));
    swan::Input input;input.interact=true;game.handleInput(input);
    require(game.collected()==1,"Collection ignored world transform");
    scene.get(c)->solid=true;
    auto motion=swan::moveCapsule(scene,{10,1,-2},{0,-0.1f,0});
    require(motion.grounded,"Collider ignored hierarchy");
    scene.get(r)->animation=swan::Animation{};rejects([&]{scene.worldTransform(c);});scene.get(r)->animation.reset();
    scene.get(r)->transform.scale.x=3;rejects([&]{scene.worldTransform(c);});scene.get(r)->transform.scale.x=2;
    require(scene.destroy(r),"Parent destruction failed");
    require(!scene.parent(c) && close(scene.worldTransform(c).position,world.position),"Delete did not preserve child world position");
    auto replacement=scene.create(root);rejects([&]{scene.setParent(c,r);});
    scene.get(replacement)->transform.scale={2,1,1};rejects([&]{scene.setParent(c,replacement);});
    require(!scene.parent(c),"Invalid scale reparent mutated tree");
    swan::Scene deep;auto first=deep.create({});auto last=first;
    for(int i=0;i<64;++i) {auto next=deep.create({});deep.setParent(next,last);last=next;}
    auto extra=deep.create({});rejects([&]{deep.setParent(extra,last);});
    std::cout<<"Hierarchy composition, persistence, collision and deletion passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
