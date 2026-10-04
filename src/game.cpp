#include "game.hpp"
#include "physics.hpp"
#include "scene_io.hpp"
#include "script_runtime.hpp"
#include <iostream>
#include <utility>
#include <algorithm>
#include <cmath>
namespace swan {
namespace { constexpr float eyeHeight=1.65f; }
Game::Game(bool overview):Game(makeGarden(),overview) {}
Game::~Game()=default;
std::vector<std::string> Game::takeMessages() {return std::exchange(messages,{});}
Game::Game(Garden initial,bool overview,std::filesystem::path source,std::filesystem::path save)
    :sourcePath(std::move(source)),savePath(std::move(save)) {
    replaceDefinition(std::move(initial));
    if(overview) overviewCamera();
}
void Game::replaceDefinition(Garden initial) {
    Garden runtime=initial; // Complete potentially throwing allocations before replacing the world.
    RenderPose pose; pose.capture(runtime.scene);
    previousPose=std::move(pose); previousTime=0;
    scriptRuntime.reset(); // Releases references into the scene being replaced.
    definition=std::move(initial); garden=std::move(runtime);
    bool scripted=false;
    for(auto id:garden.scene.entities()) scripted|=!garden.scene.get(id)->scriptId.empty();
    if(scripted) scriptRuntime=std::make_unique<ScriptRuntime>(garden.scene,[this](const std::string& line){
        messages.push_back(line);
        if(messages.size()>256) messages.erase(messages.begin());
    });
    time=0; collectedCount=0; paused=false; flight=false; view=Camera{};
    playerYaw=previousPlayerYaw=view.yaw;
    spawn(definition.spawn);
}
void Game::reloadScene() {
    if(sourcePath.empty()) replaceDefinition(definition);
    else {
        auto document=loadScene(sourcePath);
        auto loaded=gardenFromScene(std::move(document.scene),document.spawn);
        replaceDefinition(std::move(loaded));
    }
}
void Game::saveDefinition(const std::filesystem::path& path) const {
    // Export authored data, not transient animation, deleted pickups, or player progress.
    swan::saveScene(path,{definition.scene,definition.spawn});
}
void Game::spawn(glm::vec3 position) {
    feet=position; verticalVelocity=0; grounded=false; jumpPending=false;
    view.position=feet+glm::vec3(0,eyeHeight,0); previousEye=view.position;
}
void Game::overviewCamera() {
    flight=true; view.position={17,12,20}; view.yaw=-2.275f; view.pitch=-0.38f;
    previousEye=view.position; verticalVelocity=0; jumpPending=false;
}
void Game::collect() {
    EntityId nearest;
    float distance=2.2f;
    for(auto id:garden.shards) {
        auto* entity=garden.scene.get(id);
        if(!entity) continue;
        float d=glm::length(garden.scene.worldTransform(id).position-view.position);
        if(d<distance) { nearest=id; distance=d; }
    }
    if(!garden.scene.get(nearest)) return;
    // The behaviour may react (or even destroy the entity itself) before it is removed.
    if(scriptRuntime) scriptRuntime->collected(nearest);
    garden.scene.destroy(nearest);
    {
        ++collectedCount;
        if(collectedCount==int(garden.shards.size())) {
            garden.scene.assets().set("restored-core",{{1,0.7f,0.2f},3.5f});
            if(auto* core=garden.scene.get(garden.core)) core->materialId="restored-core";
        }
    }
}
void Game::handleInput(const Input& input) {
    view.look(input.look);
    if(input.reload) {
        try { reloadScene(); fileMessage="Scene reloaded"; }
        catch(const std::exception& e) { fileMessage="Reload failed (see console)"; std::cerr << "Reload: " << e.what() << '\n'; }
    }
    if(input.save) {
        if(savePath.empty()) fileMessage="Use --save-scene PATH to enable F6";
        else {
            try { saveDefinition(savePath); fileMessage="Scene definition saved"; }
            catch(const std::exception& e) { fileMessage="Save failed (see console)"; std::cerr << "Save: " << e.what() << '\n'; }
        }
    }
    if(input.pause) { paused=!paused; jumpPending=false; previousPose.capture(garden.scene); previousTime=time; previousEye=view.position; previousPlayerYaw=playerYaw; }
    if(input.reset) { flight=false; view=Camera{}; spawn(definition.spawn); }
    if(input.toggleFlight) {
        if(flight) { flight=false; view=Camera{}; spawn(definition.spawn); }
        else { flight=true; previousEye=view.position; verticalVelocity=0; jumpPending=false; }
    }
    if(input.toggleCamera) thirdPerson=!thirdPerson;
    if(input.jump && !paused && !flight) jumpPending=true;
    if(input.interact && !paused && !flight) collect();
}
void Game::fixedUpdate(float dt,const Input& input) {
    previousPlayerYaw=playerYaw;
    previousPose.capture(garden.scene); previousTime=time;
    previousEye=view.position;
    if(paused) return;
    time+=dt;
    for(auto id:garden.scene.entities()) {
        auto* entity=garden.scene.get(id);
        if(!entity->animation) continue;
        const auto& a=*entity->animation;
        entity->transform.position.y=a.baseHeight+std::sin(time*1.3f+a.phase)*a.bob;
        entity->transform.yaw=std::remainder(entity->transform.yaw+dt*a.speed,6.2831853f);
    }
    if(scriptRuntime) {
        ScriptGameState state{time,feet,grounded,flight,collectedCount,int(garden.shards.size()),input};
        scriptRuntime->update(dt,state);
    }
    glm::vec3 front=forward(view.yaw,0),right=glm::cross(front,glm::vec3(0,1,0));
    glm::vec3 movement=front*input.move.y+right*input.move.x;
    if(flight) movement.y=input.vertical;
    if(glm::length(movement)>1) movement=glm::normalize(movement);
    if(!flight && glm::length(movement)>0.0001f) playerYaw=std::atan2(-movement.z,movement.x);
    float speed=input.sprint?9.0f:4.5f;
    if(flight) {
        view.position+=movement*dt*speed;
        return;
    }
    if(jumpPending && grounded) { verticalVelocity=6; grounded=false; }
    jumpPending=false;
    verticalVelocity=std::max(verticalVelocity-18*dt,-30.0f);
    auto motion=moveCapsule(garden.scene,feet,movement*dt*speed+glm::vec3(0,verticalVelocity*dt,0));
    feet=motion.feet; grounded=motion.grounded;
    if((grounded && verticalVelocity<0) || (motion.ceiling && verticalVelocity>0)) verticalVelocity=0;
    if(feet.y<-10) spawn(definition.spawn);
    view.position=feet+glm::vec3(0,eyeHeight,0);
}
Camera Game::presentationCamera(float alpha) const {
    auto eye=glm::mix(previousEye,view.position,alpha);
    Camera camera=view;camera.position=eye;
    return thirdPerson && !flight?cameraRig.camera(garden.scene,camera,eye):camera;
}
RenderFrame Game::renderFrame(float interpolation) const {
    float alpha=std::isfinite(interpolation)?std::clamp(interpolation,0.0f,1.0f):1.0f;
    RenderFrame frame; frame.camera=presentationCamera(alpha); frame.time=glm::mix(previousTime,time,alpha);

    frame.objects.reserve(garden.scene.size());
    for(auto id:garden.scene.entities()) {
        const auto* entity=garden.scene.get(id);
        frame.objects.push_back({previousPose.worldTransform(garden.scene,id,alpha),garden.scene.assets().get(entity->materialId),garden.scene.meshes().get(entity->meshId).data,garden.scene.textures().get(garden.scene.assets().get(entity->materialId).textureId).data});
    }
    if(thirdPerson && !flight) {
        auto eye=glm::mix(previousEye,view.position,alpha);
        float yaw=previousPlayerYaw+std::remainder(playerYaw-previousPlayerYaw,6.2831853f)*alpha;
        // Temporary avatar proxy; remains presentation data, not an authored entity.
        frame.objects.push_back({{eye+glm::vec3(0,-eyeHeight+0.9f,0),{0.6f,1.8f,0.6f},yaw},{{0.2f,0.65f,0.9f},0}});
    }
    return frame;
}
std::string Game::status() const {
    std::string text=flight?"FLY | F walk":"WALK | F fly";
    if(!flight) text+=thirdPerson?" | THIRD PERSON (V)":" | FIRST PERSON (V)";
    if(!garden.shards.empty()) text+=" | shards "+std::to_string(collectedCount)+"/"+std::to_string(garden.shards.size());
    if(restored()) text+=" | Garden restored!";
    else if(!flight && !garden.shards.empty()) {
        for(auto id:garden.shards) {
            const auto* entity=garden.scene.get(id);
            if(entity && glm::length(garden.scene.worldTransform(id).position-view.position)<2.2f) {text+=" | E collect"; break;}
        }
    }
    if(paused) text+=" | PAUSED";
    if(!fileMessage.empty()) text+=" | "+fileMessage;
    if(scriptRuntime && !scriptRuntime->message().empty()) text+=" | "+scriptRuntime->message();
    return text;
}
}
