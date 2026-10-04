#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "render_frame.hpp"
#include "input.hpp"
#include "options.hpp"
#include <filesystem>
#include <string>
#include <map>
namespace swan {
class VulkanRenderer {
public:
    explicit VulkanRenderer(Options options);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    Input pollInput();
    bool shouldClose() const;
    void draw(const RenderFrame& frame);
    void setTitle(const std::string& title);
    void resize(int width, int height);
    void finish();
private:
    void initialize();
    void cleanup();
    void createSwapchain();
    void destroySwapchain();
    void createPipeline();
    void rebuild();
    struct GpuMesh {
        SharedMesh owner;
        VkBuffer buffer=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkDeviceSize indexOffset=0;
        uint32_t indexCount=0;
    };
    GpuMesh uploadMesh(SharedMesh mesh);
    void releaseMesh(GpuMesh& mesh);
    void synchronizeMeshes(const RenderFrame& frame);
    std::map<const MeshData*,GpuMesh> gpuMeshes;
    size_t uploadedMeshes=0;
    uint32_t memoryType(uint32_t mask,VkMemoryPropertyFlags flags);
    VkShaderModule shader(const char* name);
    Options options;
    GLFWwindow* window=nullptr;
    VkInstance instance=VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug=VK_NULL_HANDLE;
    VkSurfaceKHR surface=VK_NULL_HANDLE;
    VkPhysicalDevice gpu=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    uint32_t family=0;
    VkQueue queue=VK_NULL_HANDLE;
    VkSwapchainKHR swapchain=VK_NULL_HANDLE;
    VkFormat format=VK_FORMAT_UNDEFINED,depthFormat=VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> presentReady;
    VkImage depth=VK_NULL_HANDLE;
    VkDeviceMemory depthMemory=VK_NULL_HANDLE;
    VkImageView depthView=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkCommandBuffer command=VK_NULL_HANDLE;
    VkSemaphore acquired=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    Input input;
    bool captured=false;
    double lastX=0,lastY=0;
    unsigned validationErrors=0;
    friend VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT*,void*);
};
}
