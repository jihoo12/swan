#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "render_frame.hpp"
#include "game_layer.hpp"
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
    void cancelClose();
    void setCursorCaptured(bool enabled);
    void draw(const RenderFrame& frame);
    void setTitle(const std::string& title);
    void resize(int width, int height);
    void finish();
    // Headless mode (Options::headless): render one frame offscreen and read it back as RGBA8
    // sRGB pixels. Synchronous; reuses the target and readback buffer while the size is unchanged.
    TextureData renderImage(const RenderFrame& frame,glm::uvec2 size);
private:
    void initialize();
    // Headless color target read back by renderImage().
    struct SceneTarget {
        VkExtent2D extent{};
        VkImage color=VK_NULL_HANDLE;
        VkDeviceMemory colorMemory=VK_NULL_HANDLE;
        VkImageView colorView=VK_NULL_HANDLE;
    };
    SceneTarget sceneTarget;
    void createSceneTarget(VkExtent2D extent);
    void destroySceneTarget();
    VkImage createImage(VkExtent2D size,VkFormat imageFormat,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkDeviceMemory& memory,VkImageView& view);
    // Renders the scene and particles into the HDR target, then bloom and the tone-mapped
    // composite into `output` (already in COLOR_ATTACHMENT_OPTIMAL layout).
    void recordScene(const RenderFrame& frame,VkImageView output,VkExtent2D size);
    // Particle billboards: alpha batches sorted far to near, then additive batches.
    void createParticlePipelines();
    void recordParticles(const RenderFrame& frame);
    void reserveParticles(VkDeviceSize bytes);
    void releaseParticles();
    // HDR post-process: frame uniforms (set 1 of scene/particle pipelines), the float color/depth
    // target, a bloom mip chain, and the composite into the output format.
    struct Image { VkImage image=VK_NULL_HANDLE; VkDeviceMemory memory=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE; VkExtent2D extent{}; };
    struct PostTargets { VkExtent2D extent{}; Image hdr,depth; std::vector<Image> bloom; std::vector<VkDescriptorSet> sets; };
    void createFrameResources();
    void writeFrameUniforms(const RenderFrame& frame,const glm::mat4& viewProjection);
    void createPostPipelines();
    void createPostTargets(VkExtent2D extent);
    void destroyPostTargets();
    void destroyImage(Image& image);
    void recordPost(const RenderFrame& frame,VkImageView output,VkExtent2D size);
    static constexpr VkFormat hdrFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
    PostTargets post;
    VkDescriptorSetLayout frameLayout=VK_NULL_HANDLE,postSetLayout=VK_NULL_HANDLE;
    VkDescriptorPool framePool=VK_NULL_HANDLE,postPool=VK_NULL_HANDLE;
    VkDescriptorSet frameSet=VK_NULL_HANDLE;
    VkBuffer frameBuffer=VK_NULL_HANDLE;
    VkDeviceMemory frameMemory=VK_NULL_HANDLE;
    void* frameMapped=nullptr;
    VkSampler postSampler=VK_NULL_HANDLE;
    VkPipelineLayout postLayout=VK_NULL_HANDLE;
    VkPipeline postDown=VK_NULL_HANDLE,postUp=VK_NULL_HANDLE,postComposite=VK_NULL_HANDLE;
    VkPipeline particleAdditive=VK_NULL_HANDLE,particleAlpha=VK_NULL_HANDLE;
    VkBuffer particleBuffer=VK_NULL_HANDLE;
    VkDeviceMemory particleMemory=VK_NULL_HANDLE;
    void* particleMapped=nullptr;
    VkDeviceSize particleCapacity=0;
    uint64_t drawnParticles=0;
    VkBuffer readback=VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory=VK_NULL_HANDLE;
    VkDeviceSize readbackCapacity=0;
    void releaseReadback();
    uint64_t frameVisible=0,frameCulled=0;
    bool encodeSrgb=false;
    void cleanup();
    void createSwapchain();
    void destroySwapchain();
    void createPipeline();
    void rebuild();
    struct GpuTexture {
        SharedTexture owner;
        VkImage image=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkImageView view=VK_NULL_HANDLE;
        VkDescriptorSet descriptor=VK_NULL_HANDLE;
    };
    void createDescriptors();
    GpuTexture uploadTexture(SharedTexture texture);
    void releaseTexture(GpuTexture& texture);
    void synchronizeTextures(const RenderFrame& frame);
    std::map<const TextureData*,GpuTexture> gpuTextures;
    VkDescriptorSetLayout textureLayout=VK_NULL_HANDLE;
    VkDescriptorPool texturePool=VK_NULL_HANDLE;
    VkSampler textureSampler=VK_NULL_HANDLE;
    size_t uploadedTextures=0;
    struct GpuMesh {
        SharedMesh owner;
        VkBuffer buffer=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkDeviceSize indexOffset=0,byteSize=0;
        uint32_t indexCount=0;
    };
    GpuMesh uploadMesh(SharedMesh mesh);
    void releaseMesh(GpuMesh& mesh);
    void synchronizeMeshes(const RenderFrame& frame);
    std::map<const MeshData*,GpuMesh> gpuMeshes;
    size_t uploadedMeshes=0,verifiedMeshUploads=0;
    VkDeviceSize uploadedMeshBytes=0;
    uint64_t renderedFrames=0,submittedObjects=0,culledObjects=0;
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
