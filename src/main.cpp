#include "engine.hpp"
#include "game.hpp"
#include "editor_layer.hpp"
#include "scene_io.hpp"
#include "render_command.hpp"
#include "script_api.hpp"
#include <cstdlib>
#include <iostream>
#include <charconv>
int main(int argc,char** argv) {
    try {
        // Headless automation: no window, GPU, or GLFW initialization.
        if(argc>=2 && std::string(argv[1])=="script") {
            if(argc<3) throw std::runtime_error("Usage: swan script FILE.lua [ARGS...]");
            // Preview:render() creates a headless GPU renderer on first use.
            swan::LazyRenderer gpu(std::getenv("SWAN_VALIDATION")!=nullptr);
            int status=swan::runScriptFile(argv[2],std::vector<std::string>(argv+3,argv+argc),gpu.renderer());
            gpu.finish();
            return status;
        }
        if(argc>=2 && std::string(argv[1])=="render") return swan::runRenderCommand(std::vector<std::string>(argv+2,argv+argc));
        swan::Options options;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="editor" && i==1) {
                options.editor=true;
                if(i+1<argc && argv[i+1][0]!='-') options.scenePath=argv[++i];
            } else if(arg=="--help") {
                std::cout << "Swan — Vulkan 3D garden\nUsage: swan editor [SCENE] | swan script FILE.lua [ARGS...] | swan render SCENE [OPTIONS] (see swan render --help) | swan [--editor] [--validation] [--x11] [--overview] [--third-person] [--no-culling] [--verify-mesh-uploads] [--frames N] [--fps-limit N] [--resize-test] [--reload-test] [--shader-dir PATH]\nScene: [--scene PATH] [--save-scene PATH] [--export-scene PATH] [--validate-scene PATH]\n"
                          << "WASD move | Shift sprint | click to look | Space jump | E collect\nV toggle first/third person | F toggle flight (Space/Ctrl ascend/descend) | P pause | R respawn | F5 reload | F6 save definition | Esc release / quit\n";
                return 0;
            } else if(arg=="--validation") options.validation=true;
            else if(arg=="--editor") options.editor=true;
            else if(arg=="--verify-mesh-uploads") options.verifyMeshUploads=true;
            else if(arg=="--no-culling") options.culling=false;
            else if(arg=="--third-person") options.thirdPerson=true;
            else if(arg=="--overview") options.overview=true;
            else if(arg=="--x11") options.x11=true;
            else if(arg=="--reload-test") options.reloadTest=true;
            else if(arg=="--resize-test") options.resizeTest=true;
            else if(arg=="--scene" && i+1<argc) options.scenePath=argv[++i];
            else if(arg=="--save-scene" && i+1<argc) options.savePath=argv[++i];
            else if(arg=="--export-scene" && i+1<argc) options.exportPath=argv[++i];
            else if(arg=="--validate-scene" && i+1<argc) { options.scenePath=argv[++i]; options.validateScene=true; }
            else if(arg=="--shader-dir" && i+1<argc) options.shaderDir=argv[++i];
            else if(arg=="--fps-limit" && i+1<argc) {
                std::string value=argv[++i];
                auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),options.fpsLimit);
                if(ec!=std::errc{} || end!=value.data()+value.size() || options.fpsLimit<0 || options.fpsLimit>1000) throw std::runtime_error("--fps-limit requires 0 (unlimited) to 1000");
            }
            else if(arg=="--frames" && i+1<argc) {
                std::string value=argv[++i];
                auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),options.frames);
                if(ec!=std::errc{} || end!=value.data()+value.size() || options.frames<=0) throw std::runtime_error("--frames requires a positive integer");
            } else throw std::runtime_error("Unknown or incomplete argument: "+arg);
        }
        if(options.editor && !options.validateScene && options.exportPath.empty()) {
            swan::SceneDocument document;
            if(!options.scenePath.empty()) document=swan::loadScene(options.scenePath);
            swan::EditorLayer editor(std::move(document),options);
            swan::Engine engine(options);engine.run(editor);return 0;
        }
        swan::Garden level;
        if(!options.scenePath.empty()) {
            auto document=swan::loadScene(options.scenePath);
            level=swan::gardenFromScene(std::move(document.scene),document.spawn);
        } else level=swan::makeGarden();
        if(options.validateScene) {
            std::cout << "Valid scene: " << level.scene.size() << " entities, "
                      << level.scene.assets().entries().size() << " materials, "
                      << level.scene.meshes().entries().size() << " mesh assets, "
                      << level.scene.textures().entries().size() << " texture assets\n";
            return 0;
        }
        if(!options.exportPath.empty()) {
            swan::saveScene(options.exportPath,{level.scene,level.spawn});
            std::cout << "Exported scene: " << options.exportPath << '\n';
            return 0;
        }
        swan::Game game(std::move(level),options.overview,options.scenePath,options.savePath);
        game.setThirdPerson(options.thirdPerson);
        swan::Engine engine(options);
        engine.run(game);
    } catch(const std::exception& e) { std::cerr << "Swan: " << e.what() << '\n'; return 1; }
}
