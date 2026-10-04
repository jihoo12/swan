#include "renderer.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
namespace swan {
namespace {void checkGui(VkResult result) {if(result!=VK_SUCCESS) throw std::runtime_error("ImGui Vulkan error: "+std::to_string(result));}}
void VulkanRenderer::initializeGui() {
    IMGUI_CHECKVERSION();ImGui::CreateContext();guiContext=true;
    ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();
    if(!ImGui_ImplGlfw_InitForVulkan(window,true)) throw std::runtime_error("ImGui GLFW initialization failed");
    guiGlfw=true;
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,128};
    VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;info.maxSets=128;info.poolSizeCount=1;info.pPoolSizes=&size;
    checkGui(vkCreateDescriptorPool(device,&info,nullptr,&guiPool));initializeGuiVulkan();
}
void VulkanRenderer::initializeGuiVulkan() {
    ImGui_ImplVulkan_InitInfo info{};
    info.Instance=instance;info.PhysicalDevice=gpu;info.Device=device;info.QueueFamily=family;info.Queue=queue;
    info.DescriptorPool=guiPool;info.MinImageCount=2;info.ImageCount=static_cast<uint32_t>(images.size());
    info.MSAASamples=VK_SAMPLE_COUNT_1_BIT;info.UseDynamicRendering=true;info.CheckVkResultFn=checkGui;
    info.PipelineRenderingCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.PipelineRenderingCreateInfo.colorAttachmentCount=1;info.PipelineRenderingCreateInfo.pColorAttachmentFormats=&format;
    if(!ImGui_ImplVulkan_Init(&info)) throw std::runtime_error("ImGui Vulkan initialization failed");
    guiVulkan=true;
}
void VulkanRenderer::beginGui() {
    if(!guiContext) return;
    ImGui_ImplVulkan_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
}
void VulkanRenderer::shutdownGui() {
    if(guiVulkan) {ImGui_ImplVulkan_Shutdown();guiVulkan=false;}
    if(guiGlfw) {ImGui_ImplGlfw_Shutdown();guiGlfw=false;}
    if(guiPool) {vkDestroyDescriptorPool(device,guiPool,nullptr);guiPool=VK_NULL_HANDLE;}
    if(guiContext) {ImGui::DestroyContext();guiContext=false;}
}
}
