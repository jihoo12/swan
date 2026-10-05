#include "engine.hpp"
#include "fixed_step.hpp"
#include "renderer.hpp"
#include <chrono>
#include <iostream>
#include <thread>
namespace swan {
void Engine::run(GameLayer& game) {
    VulkanRenderer renderer(options);
    FixedStep clock;
    auto previous=std::chrono::steady_clock::now(),titleTime=previous;
    int frames=0,titleFrames=0;
    std::string lastStatus;
    float fps=0;
    auto deadline=std::chrono::steady_clock::now();
    std::cout << "Scene: " << game.renderFrame(1).objects.size() << " render objects\n";
    while(game.running() && (!options.frames || frames<options.frames)) {
        Input input=renderer.pollInput();
        if(renderer.shouldClose()) {
            if(game.allowClose()) break;
            renderer.cancelClose();
        }
        auto now=std::chrono::steady_clock::now();
        double elapsed=std::chrono::duration<double>(now-previous).count(); previous=now;
        if(options.reloadTest && (frames==10 || frames==30)) input.reload=true;
        game.handleInput(input);
        if(auto capture=game.cursorCapture()) renderer.setCursorCaptured(*capture);
        clock.advance(elapsed,[&](float dt){game.fixedUpdate(dt,input);});
        renderer.draw(game.renderFrame(float(clock.remainder()/FixedStep::interval)));
        ++frames; ++titleFrames;
        if(options.resizeTest && frames==10) renderer.resize(960,640);
        if(options.resizeTest && frames==30) renderer.resize(1280,800);
        for(const auto& line:game.takeMessages()) std::cout << line << '\n';
        auto status=game.status();
        if(status!=lastStatus) { std::cout << status << '\n'; lastStatus=status; }
        float titleElapsed=std::chrono::duration<float>(now-titleTime).count();
        if(titleElapsed>=0.25f || frames==1) {
            fps=titleElapsed>0?float(titleFrames)/titleElapsed:0;
            renderer.setTitle("SWAN | "+std::to_string(int(fps))+" FPS | "+status);
            titleFrames=0; titleTime=now;
        }
        // Frame pacing: sleep to the next frame slot instead of rendering as fast as possible.
        int limit=options.fpsLimit>=0?options.fpsLimit:game.frameRateLimit();
        if(limit>0) {
            auto period=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0/limit));
            deadline+=period;
            auto current=std::chrono::steady_clock::now();
            // After a stall, start a fresh schedule rather than rushing to catch up.
            if(deadline<current) deadline=current;
            else std::this_thread::sleep_until(deadline);
        } else deadline=std::chrono::steady_clock::now();
    }
    renderer.finish();
    std::cout << "Completed " << frames << " frames\n";
}
}
