#pragma once
#include <filesystem>
namespace swan {
struct Options {
    bool validation=false;
    bool x11=false;
    int frames=0;
    bool resizeTest=false,reloadTest=false;
    bool overview=false;
    std::filesystem::path shaderDir,scenePath,exportPath,savePath;
    bool validateScene=false;
};
}
