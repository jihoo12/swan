#include "renderer.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
namespace swan {
namespace {void checkGui(VkResult result) {if(result!=VK_SUCCESS) throw std::runtime_error("ImGui Vulkan error: "+std::to_string(result));}}
void VulkanRenderer::initializeGui() {
    IMGUI_CHECKVERSION();ImGui::CreateContext();guiContext=true;
    // GUI layers choose persistence, fonts, and style in GameLayer::initializeGui().
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    if(!ImGui_ImplGlfw_InitForVulkan(window,true)) throw std::runtime_error("ImGui GLFW initialization failed");
    guiGlfw=true;initializeGuiVulkan();
}
void VulkanRenderer::initializeGuiVulkan() {
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion=VK_API_VERSION_1_3;info.Instance=instance;info.PhysicalDevice=gpu;info.Device=device;info.QueueFamily=family;info.Queue=queue;
    info.DescriptorPoolSize=64;info.MinImageCount=2;info.ImageCount=std::max<uint32_t>(2,static_cast<uint32_t>(images.size()));
    info.UseDynamicRendering=true;info.CheckVkResultFn=checkGui;
    auto& rendering=info.PipelineInfoMain.PipelineRenderingCreateInfo;
    rendering.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
    if(!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("ImGui Vulkan initialization failed");
    guiVulkan=true;
}
void VulkanRenderer::beginGui() {
    if(!guiContext) return;
    // Resize the viewport image before this frame's GUI references its descriptor.
    if(requestedTarget.width && requestedTarget.height && (requestedTarget.width!=sceneTarget.extent.width || requestedTarget.height!=sceneTarget.extent.height)) {
        checkGui(vkWaitForFences(device,1,&fence,true,UINT64_MAX));
        destroySceneTarget();createSceneTarget(requestedTarget);
    }
    ImGui_ImplVulkan_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
}
GuiFrame VulkanRenderer::guiFrame(float deltaTime,float fps) const {
    GuiFrame frame;
    frame.sceneTexture=reinterpret_cast<uint64_t>(sceneTarget.texture);
    frame.sceneSize={sceneTarget.extent.width,sceneTarget.extent.height};
    frame.deltaTime=deltaTime;frame.fps=fps;frame.visibleObjects=frameVisible;frame.culledObjects=frameCulled;
    float x=1,y=1;glfwGetWindowContentScale(window,&x,&y);frame.contentScale=std::max(x,y);
    return frame;
}
void VulkanRenderer::shutdownGui() {
    if(guiVulkan) {ImGui_ImplVulkan_Shutdown();guiVulkan=false;}
    if(guiGlfw) {ImGui_ImplGlfw_Shutdown();guiGlfw=false;}
    if(guiContext) {ImGui::DestroyContext();guiContext=false;}
}
}
