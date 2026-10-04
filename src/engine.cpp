#include "engine.hpp"
#include "fixed_step.hpp"
#include "renderer.hpp"
#include <chrono>
#include <iostream>
namespace swan {
void Engine::run(GameLayer& game) {
    VulkanRenderer renderer(options);
    FixedStep clock;
    auto previous=std::chrono::steady_clock::now(),titleTime=previous;
    int frames=0,titleFrames=0;
    std::string lastStatus;
    std::cout << "Scene: " << game.renderFrame(1).objects.size() << " render objects\n";
    while(!renderer.shouldClose() && (!options.frames || frames<options.frames)) {
        Input input=renderer.pollInput();
        if(renderer.shouldClose()) break;
        auto now=std::chrono::steady_clock::now();
        double elapsed=std::chrono::duration<double>(now-previous).count(); previous=now;
        game.handleInput(input);
        clock.advance(elapsed,[&](float dt){game.fixedUpdate(dt,input);});
        renderer.draw(game.renderFrame(float(clock.remainder()/FixedStep::interval)));
        ++frames; ++titleFrames;
        if(options.resizeTest && frames==10) renderer.resize(960,640);
        if(options.resizeTest && frames==30) renderer.resize(1280,800);
        auto status=game.status();
        if(status!=lastStatus) { std::cout << status << '\n'; lastStatus=status; }
        float titleElapsed=std::chrono::duration<float>(now-titleTime).count();
        if(titleElapsed>=0.25f || frames==1) {
            auto fps=titleElapsed>0?int(titleFrames/titleElapsed):0;
            renderer.setTitle("SWAN | "+std::to_string(fps)+" FPS | "+status);
            titleFrames=0; titleTime=now;
        }
    }
    renderer.finish();
    std::cout << "Completed " << frames << " frames\n";
}
}
