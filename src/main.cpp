#include "engine.hpp"
#include <iostream>
#include <charconv>
int main(int argc,char** argv) {
    try {
        swan::Options options;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--help") {
                std::cout << "Swan — Vulkan 3D garden\nUsage: swan [--validation] [--x11] [--frames N] [--resize-test] [--shader-dir PATH]\n"
                          << "WASD move | Q/E descend/ascend | Shift sprint | left click capture mouse\nEsc release mouse / quit | Space pause | R reset camera\n";
                return 0;
            } else if(arg=="--validation") options.validation=true;
            else if(arg=="--x11") options.x11=true;
            else if(arg=="--resize-test") options.resizeTest=true;
            else if(arg=="--shader-dir" && i+1<argc) options.shaderDir=argv[++i];
            else if(arg=="--frames" && i+1<argc) {
                std::string value=argv[++i];
                auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),options.frames);
                if(ec!=std::errc{} || end!=value.data()+value.size() || options.frames<=0) throw std::runtime_error("--frames requires a positive integer");
            } else throw std::runtime_error("Unknown or incomplete argument: "+arg);
        }
        swan::Engine engine(options);
        engine.run();
    } catch(const std::exception& e) { std::cerr << "Swan: " << e.what() << '\n'; return 1; }
}
