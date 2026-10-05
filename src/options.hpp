#pragma once
#include <filesystem>
namespace swan {
struct Options {
    bool validation=false;
    bool headless=false; // No window or swapchain: VulkanRenderer::renderImage() only.
    bool culling=true;
    bool verifyMeshUploads=false;
    bool x11=false;
    int frames=0;
    int fpsLimit=-1;   // --fps-limit: overrides GameLayer::frameRateLimit() when >= 0 (0: unlimited).
    bool resizeTest=false,reloadTest=false;
    bool overview=false,thirdPerson=false;
    std::filesystem::path shaderDir,scenePath,exportPath,savePath;
    bool validateScene=false;
};
}
