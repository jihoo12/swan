#include "renderer.hpp"
namespace swan {
void VulkanRenderer::createSceneTarget(VkExtent2D size) {
    // Same color format as the composite pipeline (RGBA8 in headless mode).
    try {
        sceneTarget.extent=size;
        // Transfer source: headless renders read the image back.
        sceneTarget.color=createImage(size,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT,sceneTarget.colorMemory,sceneTarget.colorView);
    } catch(...) {destroySceneTarget();throw;}
}
void VulkanRenderer::destroySceneTarget() {
    // Callers guarantee the GPU no longer uses the target (frame fence or device idle).
    auto& t=sceneTarget;
    if(t.colorView) vkDestroyImageView(device,t.colorView,nullptr);
    if(t.color) vkDestroyImage(device,t.color,nullptr);
    if(t.colorMemory) vkFreeMemory(device,t.colorMemory,nullptr);
    t=SceneTarget{};
}
}
