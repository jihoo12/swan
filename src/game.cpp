#include "game.hpp"
#include "physics.hpp"
#include <algorithm>
#include <cmath>
namespace swan {
namespace { constexpr float eyeHeight=1.65f; }
Game::Game(bool overview) {
    spawn(feet);
    if(overview) overviewCamera();
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
        float d=glm::length(entity->transform.position-view.position);
        if(d<distance) { nearest=id; distance=d; }
    }
    if(garden.scene.destroy(nearest)) {
        ++collectedCount;
        if(collectedCount==int(garden.shards.size())) {
            if(auto* core=garden.scene.get(garden.core)) core->material={{1,0.7f,0.2f},3.5f};
        }
    }
}
void Game::handleInput(const Input& input) {
    view.look(input.look);
    if(input.pause) { paused=!paused; jumpPending=false; }
    if(input.reset) { flight=false; view=Camera{}; spawn({0,0.2f,14}); }
    if(input.toggleFlight) {
        if(flight) { flight=false; view=Camera{}; spawn({0,0.2f,14}); }
        else { flight=true; previousEye=view.position; verticalVelocity=0; jumpPending=false; }
    }
    if(input.jump && !paused && !flight) jumpPending=true;
    if(input.interact && !paused && !flight) collect();
}
void Game::fixedUpdate(float dt,const Input& input) {
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
    glm::vec3 front=forward(view.yaw,0),right=glm::cross(front,glm::vec3(0,1,0));
    glm::vec3 movement=front*input.move.y+right*input.move.x;
    if(flight) movement.y=input.vertical;
    if(glm::length(movement)>1) movement=glm::normalize(movement);
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
    if(feet.y<-10) spawn({0,0.2f,14});
    view.position=feet+glm::vec3(0,eyeHeight,0);
}
RenderFrame Game::renderFrame(float interpolation) const {
    RenderFrame frame; frame.camera=view; frame.time=time;
    frame.camera.position=glm::mix(previousEye,view.position,std::clamp(interpolation,0.0f,1.0f));
    frame.objects.reserve(garden.scene.size());
    for(auto id:garden.scene.entities()) {
        const auto* entity=garden.scene.get(id);
        frame.objects.push_back({entity->transform,entity->material});
    }
    return frame;
}
std::string Game::status() const {
    std::string text=flight?"FLY | F walk":"WALK | F fly";
    text+=" | shards "+std::to_string(collectedCount)+"/"+std::to_string(garden.shards.size());
    if(collectedCount==int(garden.shards.size())) text+=" | Garden restored!";
    else if(!flight) {
        for(auto id:garden.shards) {
            const auto* entity=garden.scene.get(id);
            if(entity && glm::length(entity->transform.position-view.position)<2.2f) {text+=" | E collect"; break;}
        }
    }
    if(paused) text+=" | PAUSED";
    return text;
}
}
