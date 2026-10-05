#include "renderer.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
namespace swan {
void VulkanRenderer::createSceneTarget(VkExtent2D size) {
    // Same color format as the window, so one composite pipeline serves both destinations.
    try {
        sceneTarget.extent=size;
        // Transfer source: headless renders read the image back.
        sceneTarget.color=createImage(size,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT,sceneTarget.colorMemory,sceneTarget.colorView);
        if(guiVulkan) sceneTarget.texture=ImGui_ImplVulkan_AddTexture(sceneTarget.colorView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } catch(...) {destroySceneTarget();throw;}
}
void VulkanRenderer::destroySceneTarget() {
    // Callers guarantee the GPU no longer uses the target (frame fence or device idle).
    auto& t=sceneTarget;
    if(t.texture && guiVulkan) ImGui_ImplVulkan_RemoveTexture(t.texture);
    if(t.colorView) vkDestroyImageView(device,t.colorView,nullptr);
    if(t.color) vkDestroyImage(device,t.color,nullptr);
    if(t.colorMemory) vkFreeMemory(device,t.colorMemory,nullptr);
    t=SceneTarget{};
}
}
