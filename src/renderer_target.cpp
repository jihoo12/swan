#include "renderer.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
namespace swan {
void VulkanRenderer::createSceneTarget(VkExtent2D size) {
    // Same color/depth formats as the window, so one scene pipeline serves both destinations.
    try {
        sceneTarget.extent=size;
        sceneTarget.color=createImage(size,format,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,sceneTarget.colorMemory,sceneTarget.colorView);
        sceneTarget.depth=createImage(size,depthFormat,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,sceneTarget.depthMemory,sceneTarget.depthView);
        sceneTarget.texture=ImGui_ImplVulkan_AddTexture(sceneTarget.colorView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } catch(...) {destroySceneTarget();throw;}
}
void VulkanRenderer::destroySceneTarget() {
    // Callers guarantee the GPU no longer uses the target (frame fence or device idle).
    auto& t=sceneTarget;
    if(t.texture && guiVulkan) ImGui_ImplVulkan_RemoveTexture(t.texture);
    if(t.colorView) vkDestroyImageView(device,t.colorView,nullptr);
    if(t.depthView) vkDestroyImageView(device,t.depthView,nullptr);
    if(t.color) vkDestroyImage(device,t.color,nullptr);
    if(t.depth) vkDestroyImage(device,t.depth,nullptr);
    if(t.colorMemory) vkFreeMemory(device,t.colorMemory,nullptr);
    if(t.depthMemory) vkFreeMemory(device,t.depthMemory,nullptr);
    t=SceneTarget{};
}
}
