#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "scene.hpp"
#include <array>
#include <filesystem>
#include <string>
namespace swan {
struct Options { bool validation=false; bool x11=false; int frames=0; bool resizeTest=false; std::filesystem::path shaderDir; };
class Engine {
public:
    explicit Engine(Options options);
    ~Engine();
    void run();
private:
    void initialize();
    void cleanup();
    void createSwapchain();
    void destroySwapchain();
    void createPipeline();
    void draw(float time);
    void update(float dt);
    void rebuild();
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
    std::vector<Object> objects=makeScene();
    glm::vec3 eye{17,12,20};
    float yaw=-2.275f,pitch=-0.38f;
    bool captured=false,paused=false;
    double lastX=0,lastY=0;
    float sceneTime=0;
    unsigned validationErrors=0;
    friend VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT*,void*);
};
}
