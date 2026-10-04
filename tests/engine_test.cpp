#include "fixed_step.hpp"
#include "game.hpp"
#include "physics.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void near(float a,float b,const char* message,float epsilon=0.002f) { require(std::abs(a-b)<epsilon,message); }
swan::Entity solid(glm::vec3 position,glm::vec3 scale,float yaw=0) {
    swan::Entity e; e.transform={position,scale,yaw}; e.solid=true; return e;
}
int main() {
    try {
        swan::Scene world;
        world.create(solid({0,-0.5f,0},{100,1,100}));
        auto floor=swan::moveCapsule(world,{0,3,0},{0,-5,0});
        near(floor.feet.y,0,"Capsule fell through floor"); require(floor.grounded,"Landing did not ground player");
        world.create(solid({1,1,0},{0.2f,2,8}));
        auto wall=swan::moveCapsule(world,{0,0,0},{5,0,2});
        require(wall.feet.x<=0.601f && wall.feet.z>1.8f,"Wall blocks/slides incorrectly or tunnels");
        swan::Scene rotated;
        rotated.create(solid({0,1,0},{0.2f,2,8},0.78539816f));
        auto diagonal=swan::moveCapsule(rotated,{-2,0,0},{4,0,0});
        float localX=0.70710678f*(diagonal.feet.x-diagonal.feet.z);
        require(localX<=-0.399f,"Rotated collider penetrated");
        swan::Scene ceiling;
        ceiling.create(solid({0,2.3f,0},{5,0.2f,5}));
        auto overhead=swan::moveCapsule(ceiling,{0,0,0},{0,2,0});
        near(overhead.feet.y,0.4f,"Capsule passed through ceiling"); require(overhead.ceiling,"Ceiling contact missing");
        swan::FixedStep clockA,clockB;
        swan::Game a,b;
        swan::Input move; move.move.y=1;
        int ticksA=0,ticksB=0;
        for(int i=0;i<60;++i) ticksA+=clockA.advance(1.0/60,[&](float dt){a.fixedUpdate(dt,move);});
        for(int i=0;i<144;++i) ticksB+=clockB.advance(1.0/144,[&](float dt){b.fixedUpdate(dt,move);});
        require(ticksA==120 && ticksB==120,"Simulation depends on display framerate");
        require(glm::distance(a.camera().position,b.camera().position)<0.00001f,"Controller depends on display framerate");
        require(a.onGround(),"Player not grounded after walking");
        auto groundedY=a.camera().position.y;
        swan::Input jump; jump.jump=true;
        a.handleInput(jump); a.fixedUpdate(1.0f/120,{});
        require(a.camera().position.y>groundedY && !a.onGround(),"Jump failed");
        for(int i=0;i<180;++i) a.fixedUpdate(1.0f/120,{});
        near(a.camera().position.y,groundedY,"Jump did not land");
        auto before=a.renderFrame();
        swan::Input pause; pause.pause=true; a.handleInput(pause);
        for(int i=0;i<30;++i) a.fixedUpdate(1.0f/120,move);
        auto after=a.renderFrame();
        require(after.time==before.time && a.camera().position==before.camera.position,"Pause advanced simulation");
        a.handleInput(pause);
        std::vector<glm::vec3> shards;
        for(auto id:a.scene().entities()) if(a.scene().get(id)->collectible) shards.push_back(a.scene().get(id)->transform.position);
        auto count=a.scene().size();
        swan::Input interact; interact.interact=true;
        for(auto position:shards) {
            a.spawn({position.x,0.2f,position.z+0.6f}); a.handleInput(interact);
            a.handleInput(interact); // A stale shard handle must not count twice.
        }
        require(a.collected()==5 && a.scene().size()==count-5,"Collection lifecycle/progress wrong");
        require(a.status().find("restored")!=std::string::npos,"Completion state missing");
        swan::Game spectator(true);
        auto previous=spectator.camera().position;
        spectator.fixedUpdate(1.0f/120,move);
        auto halfway=spectator.renderFrame(0.5f);
        require(glm::distance(halfway.camera.position,(previous+spectator.camera().position)*0.5f)<0.00001f,
                "Camera interpolation does not bridge simulation states");
        auto clipped=a.renderFrame(-2);
        require(std::isfinite(clipped.camera.position.x),"Invalid interpolated camera");
        int stalled=clockA.advance(10,[](float){});
        require(stalled<=30 && clockA.remainder()<swan::FixedStep::interval,"Long frame caused unbounded catch-up");
        swan::Input fly; fly.toggleFlight=true; a.handleInput(fly);
        require(a.flying(),"Flight toggle failed");
        a.handleInput(fly); require(!a.flying(),"Walk toggle failed");
        std::cout << "Collision, fixed-step, jump, pause and collection tests passed\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
