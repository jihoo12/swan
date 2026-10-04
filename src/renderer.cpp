#include "renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <cstddef>
#include <stdexcept>
namespace swan {
namespace {
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
struct Push { glm::mat4 vp; glm::vec4 positionUvV,scaleGlow,color,eyeUvU; };
static_assert(sizeof(Push)==128);
VkImageMemoryBarrier barrier(VkImage image,VkImageAspectFlags aspect,VkImageLayout oldLayout,VkImageLayout newLayout,VkAccessFlags src,VkAccessFlags dst) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask=src; b.dstAccessMask=dst; b.oldLayout=oldLayout; b.newLayout=newLayout;
    b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image; b.subresourceRange={aspect,0,1,0,1}; return b;
}
}
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void* user) {
    if(severity>=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) std::cerr << "Vulkan: " << data->pMessage << '\n';
    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++static_cast<VulkanRenderer*>(user)->validationErrors;
    return VK_FALSE;
}
VulkanRenderer::VulkanRenderer(Options o):options(std::move(o)) { try { initialize(); } catch(...) { cleanup(); throw; } }
VulkanRenderer::~VulkanRenderer() { cleanup(); }
void VulkanRenderer::initialize() {
    glfwSetErrorCallback([](int code,const char* text){ std::cerr << "GLFW " << code << ": " << text << '\n'; });
    if(options.x11) glfwInitHint(GLFW_PLATFORM,GLFW_PLATFORM_X11);
    if(!glfwInit()) throw std::runtime_error("Cannot initialize GLFW. Run inside a graphical desktop or Xvfb.");
    if(!glfwVulkanSupported()) throw std::runtime_error("Vulkan loader/driver unavailable. Check vulkaninfo --summary.");
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    window=glfwCreateWindow(1280,800,"SWAN",nullptr,nullptr);
    if(!window) throw std::runtime_error("Cannot create window");
    glfwSetWindowUserPointer(window,this);
    glfwSetKeyCallback(window,[](GLFWwindow* w,int key,int,int action,int){
        auto& e=*static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(w));
        if(action!=GLFW_PRESS) return;
        if(key==GLFW_KEY_ESCAPE) {
            if(e.captured) { e.captured=false; glfwSetInputMode(w,GLFW_CURSOR,GLFW_CURSOR_NORMAL); }
            else glfwSetWindowShouldClose(w,GLFW_TRUE);
        }
        if(key==GLFW_KEY_SPACE) e.input.jump=true;
        if(key==GLFW_KEY_P) e.input.pause=true;
        if(key==GLFW_KEY_R) e.input.reset=true;
        if(key==GLFW_KEY_F) e.input.toggleFlight=true;
        if(key==GLFW_KEY_E) e.input.interact=true;
        if(key==GLFW_KEY_F5) e.input.reload=true;
        if(key==GLFW_KEY_F6) e.input.save=true;
    });
    glfwSetMouseButtonCallback(window,[](GLFWwindow* w,int button,int action,int){
        auto& e=*static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(w));
        if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS) {
            e.captured=true; glfwGetCursorPos(w,&e.lastX,&e.lastY); glfwSetInputMode(w,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
        }
    });
    glfwSetWindowFocusCallback(window,[](GLFWwindow* w,int focused){
        if(!focused) { static_cast<VulkanRenderer*>(glfwGetWindowUserPointer(w))->captured=false; glfwSetInputMode(w,GLFW_CURSOR,GLFW_CURSOR_NORMAL); }
    });
    uint32_t count=0; const char** required=glfwGetRequiredInstanceExtensions(&count);
    if(!required) throw std::runtime_error("No Vulkan window-system extensions");
    std::vector<const char*> extensions(required,required+count);
    const char* layer="VK_LAYER_KHRONOS_validation";
    if(options.validation) {
        uint32_t n=0; check(vkEnumerateInstanceLayerProperties(&n,nullptr),"Enumerate layers");
        std::vector<VkLayerProperties> layers(n); check(vkEnumerateInstanceLayerProperties(&n,layers.data()),"Enumerate layers");
        if(std::none_of(layers.begin(),layers.end(),[&](auto& l){return std::strcmp(l.layerName,layer)==0;})) throw std::runtime_error("Validation layer missing; enter nix develop");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="Swan"; app.applicationVersion=VK_MAKE_VERSION(0,7,0); app.pEngineName="Swan"; app.apiVersion=VK_API_VERSION_1_3;
    VkDebugUtilsMessengerCreateInfoEXT dbg{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    dbg.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg.pfnUserCallback=debugCallback; dbg.pUserData=this;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo=&app;
    info.enabledExtensionCount=uint32_t(extensions.size()); info.ppEnabledExtensionNames=extensions.data();
    if(options.validation) { info.enabledLayerCount=1; info.ppEnabledLayerNames=&layer; info.pNext=&dbg; }
    check(vkCreateInstance(&info,nullptr,&instance),"Create Vulkan 1.3 instance");
    if(options.validation) {
        auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));
        if(!create) throw std::runtime_error("Debug utils unavailable");
        check(create(instance,&dbg,nullptr,&debug),"Create validation messenger");
    }
    check(glfwCreateWindowSurface(instance,window,nullptr,&surface),"Create surface");
    uint32_t n=0; check(vkEnumeratePhysicalDevices(instance,&n,nullptr),"Enumerate GPUs");
    std::vector<VkPhysicalDevice> devices(n); check(vkEnumeratePhysicalDevices(instance,&n,devices.data()),"Enumerate GPUs");
    int best=-1;
    for(auto candidate:devices) {
        VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(candidate,&props);
        if(props.apiVersion<VK_API_VERSION_1_3) continue;
        VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; f.pNext=&f13; vkGetPhysicalDeviceFeatures2(candidate,&f);
        if(!f13.dynamicRendering) continue;
        uint32_t en=0; check(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&en,nullptr),"Device extensions");
        std::vector<VkExtensionProperties> ex(en); check(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&en,ex.data()),"Device extensions");
        if(std::none_of(ex.begin(),ex.end(),[](auto& e){return std::strcmp(e.extensionName,VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0;})) continue;
        uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(candidate,&qn,nullptr);
        std::vector<VkQueueFamilyProperties> qs(qn); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&qn,qs.data());
        for(uint32_t q=0;q<qn;++q) {
            VkBool32 present=false; check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate,q,surface,&present),"Surface support");
            int score=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?100:10;
            if(present && (qs[q].queueFlags&VK_QUEUE_GRAPHICS_BIT) && score>best) { gpu=candidate; family=q; best=score; }
        }
    }
    if(!gpu) throw std::runtime_error("No Vulkan 1.3 GPU with dynamic rendering and graphics/present queue");
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(gpu,&props); std::cout << "GPU: " << props.deviceName << '\n';
    float priority=1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=family; qi.queueCount=1; qi.pQueuePriorities=&priority;
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}; f13.dynamicRendering=true;
    const char* deviceExtension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.pNext=&f13; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi; di.enabledExtensionCount=1; di.ppEnabledExtensionNames=&deviceExtension;
    check(vkCreateDevice(gpu,&di,nullptr,&device),"Create device"); vkGetDeviceQueue(device,family,0,&queue);
    for(auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D16_UNORM}) {
        VkFormatProperties p; vkGetPhysicalDeviceFormatProperties(gpu,candidate,&p);
        if(p.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { depthFormat=candidate; break; }
    }
    if(depthFormat==VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported depth format");
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex=family; pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vkCreateCommandPool(device,&pi,nullptr,&pool),"Command pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool=pool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=1;
    check(vkAllocateCommandBuffers(device,&ai,&command),"Command buffer");
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; check(vkCreateSemaphore(device,&si,nullptr,&acquired),"Acquire semaphore");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags=VK_FENCE_CREATE_SIGNALED_BIT; check(vkCreateFence(device,&fi,nullptr,&fence),"Frame fence");
    if(options.shaderDir.empty()) {
        std::error_code ec;
        auto executable=std::filesystem::read_symlink("/proc/self/exe",ec);
        auto installed=executable.parent_path()/"../share/swan/shaders";
        options.shaderDir=(!ec && std::filesystem::exists(installed))?installed:std::filesystem::path(SWAN_SHADER_DIR);
    }
    createDescriptors(); createSwapchain(); createPipeline();

}
uint32_t VulkanRenderer::memoryType(uint32_t mask,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p; vkGetPhysicalDeviceMemoryProperties(gpu,&p);
    for(uint32_t i=0;i<p.memoryTypeCount;++i) if((mask&(1u<<i)) && (p.memoryTypes[i].propertyFlags&flags)==flags) return i;
    throw std::runtime_error("No suitable GPU memory");
}
void VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps; check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu,surface,&caps),"Surface capabilities");
    uint32_t n=0; check(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,nullptr),"Surface formats");
    if(!n) throw std::runtime_error("No surface formats");
    std::vector<VkSurfaceFormatKHR> formats(n); check(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,formats.data()),"Surface formats");
    auto selected=formats[0];
    for(auto f:formats) if(f.format==VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {selected=f;break;}
    if(selected.format==VK_FORMAT_UNDEFINED) selected.format=VK_FORMAT_B8G8R8A8_SRGB;
    format=selected.format;
    int w,h; glfwGetFramebufferSize(window,&w,&h);
    extent=caps.currentExtent;
    if(extent.width==UINT32_MAX) extent={std::clamp(uint32_t(w),caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(uint32_t(h),caps.minImageExtent.height,caps.maxImageExtent.height)};
    uint32_t imageCount=caps.minImageCount+1;
    if(caps.maxImageCount) imageCount=std::min(imageCount,caps.maxImageCount);
    VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sc.surface=surface; sc.minImageCount=imageCount; sc.imageFormat=format; sc.imageColorSpace=selected.colorSpace;
    sc.imageExtent=extent; sc.imageArrayLayers=1; sc.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    sc.preTransform=caps.currentTransform; sc.presentMode=VK_PRESENT_MODE_FIFO_KHR; sc.clipped=true;
    for(auto mode:{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) if(caps.supportedCompositeAlpha&mode) { sc.compositeAlpha=mode; break; }
    check(vkCreateSwapchainKHR(device,&sc,nullptr,&swapchain),"Swapchain");
    check(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr),"Swapchain images"); images.resize(n);
    check(vkGetSwapchainImagesKHR(device,swapchain,&n,images.data()),"Swapchain images");
    views.resize(n); presentReady.resize(n);
    for(uint32_t i=0;i<n;++i) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=images[i]; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=format; vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        check(vkCreateImageView(device,&vi,nullptr,&views[i]),"Swapchain image view");
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; check(vkCreateSemaphore(device,&si,nullptr,&presentReady[i]),"Present semaphore");
    }
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ii.imageType=VK_IMAGE_TYPE_2D; ii.format=depthFormat; ii.extent={extent.width,extent.height,1};
    ii.mipLevels=1; ii.arrayLayers=1; ii.samples=VK_SAMPLE_COUNT_1_BIT; ii.tiling=VK_IMAGE_TILING_OPTIMAL; ii.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    check(vkCreateImage(device,&ii,nullptr,&depth),"Depth image");
    VkMemoryRequirements req; vkGetImageMemoryRequirements(device,depth,&req);
    VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; mi.allocationSize=req.size; mi.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device,&mi,nullptr,&depthMemory),"Depth memory"); check(vkBindImageMemory(device,depth,depthMemory,0),"Bind depth memory");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=depth; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=depthFormat; vi.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
    check(vkCreateImageView(device,&vi,nullptr,&depthView),"Depth view");
}
VkShaderModule VulkanRenderer::shader(const char* name) {
    auto path=options.shaderDir/name; std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("Cannot open shader: "+path.string());
    auto size=file.tellg(); if(size<=0 || size%4!=0) throw std::runtime_error("Invalid SPIR-V: "+path.string());
    std::vector<uint32_t> data(static_cast<size_t>(size)/4); file.seekg(0); file.read(reinterpret_cast<char*>(data.data()),size);
    if(!file) throw std::runtime_error("Cannot read shader: "+path.string());
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; ci.codeSize=static_cast<size_t>(size); ci.pCode=data.data();
    VkShaderModule module; check(vkCreateShaderModule(device,&ci,nullptr,&module),"Shader module"); return module;
}
void VulkanRenderer::createPipeline() {
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push)};
    if(!layout) { VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.setLayoutCount=1; li.pSetLayouts=&textureLayout; li.pushConstantRangeCount=1; li.pPushConstantRanges=&range; check(vkCreatePipelineLayout(device,&li,nullptr,&layout),"Pipeline layout"); }
    VkShaderModule vert=shader("scene.vert.spv"),frag=VK_NULL_HANDLE;
    try {
        frag=shader("scene.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};
        stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",nullptr};
        static_assert(sizeof(Vertex)==32 && offsetof(Vertex,normal)==12 && offsetof(Vertex,uv)==24);
        VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[3]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)},
                                                       {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)},
                                                       {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)}};
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex.vertexBindingDescriptionCount=1; vertex.pVertexBindingDescriptions=&binding;
        vertex.vertexAttributeDescriptionCount=3; vertex.pVertexAttributeDescriptions=attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=1; viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo sample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; sample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable=true; ds.depthWriteEnable=true; ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{}; attachment.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&attachment;
        VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount=1; rendering.pColorAttachmentFormats=&format; rendering.depthAttachmentFormat=depthFormat;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext=&rendering; ci.stageCount=2; ci.pStages=stages; ci.pVertexInputState=&vertex; ci.pInputAssemblyState=&assembly; ci.pViewportState=&viewport; ci.pRasterizationState=&raster; ci.pMultisampleState=&sample; ci.pDepthStencilState=&ds; ci.pColorBlendState=&blend; ci.pDynamicState=&dynamic; ci.layout=layout;
        check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"Graphics pipeline");
    } catch(...) { if(frag) vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr); throw; }
    vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr);
}
void VulkanRenderer::destroySwapchain() {
    if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
    pipeline=VK_NULL_HANDLE;
    if(depthView) vkDestroyImageView(device,depthView,nullptr);
    depthView=VK_NULL_HANDLE;
    if(depth) vkDestroyImage(device,depth,nullptr);
    depth=VK_NULL_HANDLE;
    if(depthMemory) vkFreeMemory(device,depthMemory,nullptr);
    depthMemory=VK_NULL_HANDLE;
    for(auto v:views) if(v) vkDestroyImageView(device,v,nullptr);
    views.clear();
    for(auto s:presentReady) if(s) vkDestroySemaphore(device,s,nullptr);
    presentReady.clear();
    if(swapchain) vkDestroySwapchainKHR(device,swapchain,nullptr);
    swapchain=VK_NULL_HANDLE;
}
void VulkanRenderer::rebuild() {
    int w=0,h=0; glfwGetFramebufferSize(window,&w,&h);
    while((w==0 || h==0) && !glfwWindowShouldClose(window)) { glfwWaitEvents(); glfwGetFramebufferSize(window,&w,&h); }
    if(glfwWindowShouldClose(window)) return;
    check(vkDeviceWaitIdle(device),"Wait before resize"); destroySwapchain(); createSwapchain(); createPipeline();
}
Input VulkanRenderer::pollInput() {
    glfwPollEvents();
    if(captured) {
        double x,y;
        glfwGetCursorPos(window,&x,&y);
        input.look={float(x-lastX)*0.0025f,float(lastY-y)*0.0025f};
        lastX=x; lastY=y;
    }
    auto pressed=[&](int key){ return glfwGetWindowAttrib(window,GLFW_FOCUSED) && glfwGetKey(window,key)==GLFW_PRESS; };
    input.move={float(pressed(GLFW_KEY_D)-pressed(GLFW_KEY_A)),float(pressed(GLFW_KEY_W)-pressed(GLFW_KEY_S))};
    input.vertical=float(pressed(GLFW_KEY_SPACE)-pressed(GLFW_KEY_LEFT_CONTROL));
    input.sprint=pressed(GLFW_KEY_LEFT_SHIFT);
    Input result=input;
    input.look={}; input.jump=false; input.pause=false; input.reset=false; input.toggleFlight=false; input.interact=false; input.reload=false; input.save=false;
    return result;
}
bool VulkanRenderer::shouldClose() const { return glfwWindowShouldClose(window); }
void VulkanRenderer::setTitle(const std::string& title) { glfwSetWindowTitle(window,title.c_str()); }
void VulkanRenderer::resize(int width,int height) { glfwSetWindowSize(window,width,height); }
void VulkanRenderer::finish() {
    check(vkDeviceWaitIdle(device),"Wait shutdown");
    std::cout << "Mesh uploads: " << uploadedMeshes << "; resident meshes: " << gpuMeshes.size() << '\n';
    std::cout << "Texture uploads: " << uploadedTextures << "; resident textures: " << gpuTextures.size() << '\n';
    std::cout << "Validation errors: " << validationErrors << '\n';
    if(validationErrors) throw std::runtime_error("Vulkan validation reported errors");
}
VulkanRenderer::GpuMesh VulkanRenderer::uploadMesh(SharedMesh data) {
    if(!data || data->vertices.empty() || data->indices.empty()) throw std::invalid_argument("Cannot upload an empty mesh");
    GpuMesh mesh; mesh.owner=std::move(data);
    mesh.indexOffset=mesh.owner->vertices.size()*sizeof(Vertex);
    mesh.indexCount=uint32_t(mesh.owner->indices.size());
    try {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=mesh.indexOffset+mesh.owner->indices.size()*sizeof(uint32_t);
        info.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&info,nullptr,&mesh.buffer),"Create mesh buffer");
        VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,mesh.buffer,&requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        check(vkAllocateMemory(device,&allocation,nullptr,&mesh.memory),"Allocate mesh memory");
        check(vkBindBufferMemory(device,mesh.buffer,mesh.memory,0),"Bind mesh memory");
        void* mapped=nullptr;
        check(vkMapMemory(device,mesh.memory,0,VK_WHOLE_SIZE,0,&mapped),"Map mesh memory");
        std::memcpy(mapped,mesh.owner->vertices.data(),size_t(mesh.indexOffset));
        std::memcpy(static_cast<char*>(mapped)+mesh.indexOffset,mesh.owner->indices.data(),mesh.owner->indices.size()*sizeof(uint32_t));
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory=mesh.memory; range.offset=0; range.size=VK_WHOLE_SIZE;
        auto result=vkFlushMappedMemoryRanges(device,1,&range);
        vkUnmapMemory(device,mesh.memory); check(result,"Flush mesh memory");
    } catch(...) { releaseMesh(mesh); throw; }
    return mesh;
}
void VulkanRenderer::releaseMesh(GpuMesh& mesh) {
    if(mesh.buffer) vkDestroyBuffer(device,mesh.buffer,nullptr);
    if(mesh.memory) vkFreeMemory(device,mesh.memory,nullptr);
    mesh.buffer=VK_NULL_HANDLE; mesh.memory=VK_NULL_HANDLE;
}
void VulkanRenderer::synchronizeMeshes(const RenderFrame& frame) {
    // Called only after the frame fence: no old buffers remain in GPU use.
    std::set<const MeshData*> needed;
    for(const auto& object:frame.objects) {
        if(!object.mesh) throw std::invalid_argument("Render object has no mesh");
        needed.insert(object.mesh.get());
    }
    for(auto it=gpuMeshes.begin();it!=gpuMeshes.end();) {
        if(!needed.contains(it->first)) { releaseMesh(it->second); it=gpuMeshes.erase(it); }
        else ++it;
    }
    for(const auto& object:frame.objects) if(!gpuMeshes.contains(object.mesh.get())) {
        auto mesh=uploadMesh(object.mesh);
        try { gpuMeshes.emplace(object.mesh.get(),mesh); ++uploadedMeshes; }
        catch(...) { releaseMesh(mesh); throw; }
    }
}
void VulkanRenderer::draw(const RenderFrame& frame) {
    check(vkWaitForFences(device,1,&fence,true,UINT64_MAX),"Wait frame");
    int w,h; glfwGetFramebufferSize(window,&w,&h);
    if(w==0 || h==0 || uint32_t(w)!=extent.width || uint32_t(h)!=extent.height) { rebuild(); return; }
    synchronizeMeshes(frame);
    synchronizeTextures(frame);
    uint32_t index;
    VkResult result=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,acquired,VK_NULL_HANDLE,&index);
    if(result==VK_ERROR_OUT_OF_DATE_KHR) { rebuild(); return; }
    if(result!=VK_SUBOPTIMAL_KHR) check(result,"Acquire image");
    bool suboptimal=result==VK_SUBOPTIMAL_KHR;
    check(vkResetCommandBuffer(command,0),"Reset command");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(command,&begin),"Begin command");
    auto colorBarrier=barrier(images[index],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    auto depthBarrier=barrier(depth,VK_IMAGE_ASPECT_DEPTH_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,0,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    if(depthFormat==VK_FORMAT_D24_UNORM_S8_UINT) depthBarrier.subresourceRange.aspectMask|=VK_IMAGE_ASPECT_STENCIL_BIT;
    VkImageMemoryBarrier barriers[]={colorBarrier,depthBarrier};
    // Match the acquire semaphore wait stage so the layout transition waits for presentation.
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,2,barriers);
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; color.imageView=views[index]; color.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp=VK_ATTACHMENT_STORE_OP_STORE; color.clearValue.color={{0.035f,0.065f,0.095f,1}};
    VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; depthAttachment.imageView=depthView; depthAttachment.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; depthAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; depthAttachment.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE; depthAttachment.clearValue.depthStencil={1,0};
    VkRenderingInfo render{VK_STRUCTURE_TYPE_RENDERING_INFO}; render.renderArea={{0,0},extent}; render.layerCount=1; render.colorAttachmentCount=1; render.pColorAttachments=&color; render.pDepthAttachment=&depthAttachment;
    vkCmdBeginRendering(command,&render); vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1}; VkRect2D scissor{{0,0},extent};
    vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
    Push push{};
    push.vp=frame.camera.viewProjection(float(extent.width)/float(extent.height));
    push.eyeUvU=glm::vec4(frame.camera.position,1);
    for(const auto& object:frame.objects) {
        push.positionUvV=glm::vec4(object.transform.position,object.material.uvScale.y);
        push.scaleGlow=glm::vec4(object.transform.scale,object.material.emission);
        push.eyeUvU.w=object.material.uvScale.x;
        push.color=glm::vec4(object.material.color,object.transform.yaw);
        vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push),&push);
        const auto& mesh=gpuMeshes.at(object.mesh.get());
        VkDeviceSize offset=0;
        vkCmdBindVertexBuffers(command,0,1,&mesh.buffer,&offset);
        vkCmdBindIndexBuffer(command,mesh.buffer,mesh.indexOffset,VK_INDEX_TYPE_UINT32);
        const auto& texture=gpuTextures.at(object.texture.get());
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&texture.descriptor,0,nullptr);
        vkCmdDrawIndexed(command,mesh.indexCount,1,0,0,0);
    }
    vkCmdEndRendering(command);
    auto presentBarrier=barrier(images[index],VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,0);
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&presentBarrier);
    check(vkEndCommandBuffer(command),"End command");
    VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount=1; submit.pWaitSemaphores=&acquired; submit.pWaitDstStageMask=&waitStage; submit.commandBufferCount=1; submit.pCommandBuffers=&command; submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&presentReady[index];
    check(vkResetFences(device,1,&fence),"Reset frame fence"); check(vkQueueSubmit(queue,1,&submit,fence),"Submit frame");
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; present.waitSemaphoreCount=1; present.pWaitSemaphores=&presentReady[index]; present.swapchainCount=1; present.pSwapchains=&swapchain; present.pImageIndices=&index;
    result=vkQueuePresentKHR(queue,&present);
    if(result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR || suboptimal) rebuild(); else check(result,"Present");
}
void VulkanRenderer::cleanup() {
    if(device) {
        vkDeviceWaitIdle(device); destroySwapchain();
        for(auto& [key,mesh]:gpuMeshes) { (void)key; releaseMesh(mesh); }
        gpuMeshes.clear();
        for(auto& [key,texture]:gpuTextures) { (void)key; releaseTexture(texture); }
        gpuTextures.clear();
        if(texturePool) vkDestroyDescriptorPool(device,texturePool,nullptr);
        if(textureSampler) vkDestroySampler(device,textureSampler,nullptr);
        if(layout) vkDestroyPipelineLayout(device,layout,nullptr);
        if(textureLayout) vkDestroyDescriptorSetLayout(device,textureLayout,nullptr);
        if(fence) vkDestroyFence(device,fence,nullptr);
        if(acquired) vkDestroySemaphore(device,acquired,nullptr);
        if(pool) vkDestroyCommandPool(device,pool,nullptr);
        vkDestroyDevice(device,nullptr); device=VK_NULL_HANDLE;
    }
    if(surface) vkDestroySurfaceKHR(instance,surface,nullptr);
    if(debug) { auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT")); if(destroy) destroy(instance,debug,nullptr); }
    if(instance) vkDestroyInstance(instance,nullptr);
    if(window) glfwDestroyWindow(window);
    glfwTerminate();
}
}
