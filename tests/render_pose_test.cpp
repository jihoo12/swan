#include "render_pose.hpp"
#include "game.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
bool close(glm::vec3 a,glm::vec3 b) {return glm::length(a-b)<0.0001f;}
bool near(float a,float b) {return std::abs(a-b)<0.0001f;}
int main() {try {
    swan::Scene scene; swan::Entity entity;entity.key="root";entity.goal=true;
    auto root=scene.create(entity);
    entity.key="child";entity.goal=false;entity.collectible=true;entity.transform.position={2,0,0};
    auto child=scene.create(entity);scene.setParent(child,root);
    swan::RenderPose pose;pose.capture(scene);
    scene.get(root)->transform.yaw=1.5707963268f;
    auto half=pose.worldTransform(scene,child,0.5f);
    require(close(half.position,{1.41421356f,0,-1.41421356f}),"Hierarchy interpolation cut across orbit");
    require(close(pose.worldTransform(scene,child,0).position,{2,0,0}),"Previous endpoint incorrect");
    require(close(pose.worldTransform(scene,child,1).position,{0,0,-2}),"Current endpoint incorrect");
    require(close(pose.worldTransform(scene,child,-1).position,{2,0,0}),"Lower clamp incorrect");
    require(close(pose.worldTransform(scene,child,2).position,{0,0,-2}),"Upper clamp incorrect");
    require(close(pose.worldTransform(scene,child,std::numeric_limits<float>::quiet_NaN()).position,{0,0,-2}),"NaN factor not snapped");
    scene.get(root)->transform.yaw=3.1f;pose.capture(scene);scene.get(root)->transform.yaw=-3.1f;
    require(near(std::abs(pose.worldTransform(scene,root,0.5f).yaw),3.14159265f),"Yaw wraparound took long path");
    scene.get(root)->transform.yaw=0;pose.capture(scene);
    scene.get(root)->transform.position={4,2,0};scene.get(root)->transform.scale={3,3,3};
    require(close(pose.worldTransform(scene,root,0.5f).position,{2,1,0}) && close(pose.worldTransform(scene,root,0.5f).scale,{2,2,2}),"Position/scale interpolation incorrect");
    auto before=scene.worldTransform(child);pose.capture(scene);scene.destroy(root);
    require(close(pose.worldTransform(scene,child,0).position,before.position),"Parent deletion interpolated incompatible coordinates");
    auto reused=scene.create({});scene.get(reused)->transform.position={90,0,0};
    require(close(pose.worldTransform(scene,reused,0).position,{90,0,0}),"Reused slot inherited stale pose");
    auto fresh=scene.create({});scene.get(fresh)->transform.position={0,20,0};
    require(close(pose.worldTransform(scene,fresh,0).position,{0,20,0}),"New entity inherited history");
    auto grandchild=scene.create({});scene.setParent(grandchild,child);pose.capture(scene);
    scene.setParent(child,reused);
    require(close(pose.worldTransform(scene,grandchild,0).position,scene.worldTransform(grandchild).position),"Changed ancestor did not snap subtree");
    // Exercise the game lifecycle, including animation, pause, collection and reload.
    swan::Scene animated;
    swan::Entity core;core.goal=true;core.animation=swan::Animation{0,0,0.5f,1};
    core.transform.position={0,0,0};auto coreId=animated.create(core);
    swan::Entity shard;shard.collectible=true;shard.transform.position={2,0,0};
    auto shardId=animated.create(shard);animated.setParent(shardId,coreId);
    swan::Game game(swan::gardenFromScene(animated,{}),false);
    auto initial=game.renderFrame();game.fixedUpdate(0.1f,{});
    auto previous=game.renderFrame(0),current=game.renderFrame(1),mid=game.renderFrame(0.5f);
    require(close(previous.objects[0].transform.position,initial.objects[0].transform.position),"Game previous pose not captured");
    require(near(mid.objects[0].transform.position.y,current.objects[0].transform.position.y*0.5f) && near(mid.objects[0].transform.yaw,0.05f),"Game did not interpolate animation");
    require(near(mid.time,0.05f),"Render clock not interpolated");
    auto simulation=game.scene().worldTransform(shardId);
    require(close(current.objects[1].transform.position,simulation.position),"Rendering changed simulation pose");
    swan::Input pause;pause.pause=true;game.handleInput(pause);
    require(close(game.renderFrame(0).objects[1].transform.position,simulation.position),"Pause snapped backward");
    game.fixedUpdate(0.1f,{});
    require(near(game.renderFrame(0.5f).time,0.1f),"Pause advanced animation clock");
    game.handleInput(pause);
    game.spawn(simulation.position-glm::vec3(0,1.65f,0));swan::Input collect;collect.interact=true;game.handleInput(collect);
    require(game.renderFrame(0).objects.size()==1,"Deleted entity persisted in render history");
    game.reloadScene();
    require(game.renderFrame(0).objects.size()==2 && near(game.renderFrame(0).objects[0].transform.yaw,0),"Reload retained old pose");
    require(close(game.renderFrame(0).camera.position,game.camera().position),"Reload retained camera history");
    std::cout<<"Pose interpolation, hierarchy, wraparound and lifecycle passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
