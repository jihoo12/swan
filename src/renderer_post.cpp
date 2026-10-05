#include "renderer.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
// std140 layout of shaders/frame.glsl.
struct FrameUniforms { glm::mat4 viewProjection; glm::vec4 eye,right,up,fog,light; };
static_assert(sizeof(FrameUniforms)==144);
struct PostPush { glm::vec4 texel,params; glm::ivec4 mode; };
static_assert(sizeof(PostPush)==48);
constexpr uint32_t maxBloomLevels=6;
VkImageMemoryBarrier transition(VkImage image,VkImageAspectFlags aspect,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask=src; b.dstAccessMask=dst; b.oldLayout=before; b.newLayout=after;
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image; b.subresourceRange={aspect,0,1,0,1}; return b;
}
}
void VulkanRenderer::createFrameResources() {
    VkFormatProperties properties; vkGetPhysicalDeviceFormatProperties(gpu,hdrFormat,&properties);
    VkFormatFeatureFlags needed=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if((properties.optimalTilingFeatures&needed)!=needed) throw std::runtime_error("GPU lacks blendable, filterable RGBA16F render targets");
    VkDescriptorSetLayoutBinding uniform{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo frameInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; frameInfo.bindingCount=1; frameInfo.pBindings=&uniform;
    checked(vkCreateDescriptorSetLayout(device,&frameInfo,nullptr,&frameLayout),"Frame descriptor layout");
    VkDescriptorSetLayoutBinding samplers[2]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
                                              {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo postInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; postInfo.bindingCount=2; postInfo.pBindings=samplers;
    checked(vkCreateDescriptorSetLayout(device,&postInfo,nullptr,&postSetLayout),"Post descriptor layout");
    VkDescriptorPoolSize frameSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
    VkDescriptorPoolCreateInfo framePoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; framePoolInfo.maxSets=1; framePoolInfo.poolSizeCount=1; framePoolInfo.pPoolSizes=&frameSize;
    checked(vkCreateDescriptorPool(device,&framePoolInfo,nullptr,&framePool),"Frame descriptor pool");
    VkDescriptorPoolSize postSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2*(2*maxBloomLevels+2)};
    VkDescriptorPoolCreateInfo postPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; postPoolInfo.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    postPoolInfo.maxSets=2*maxBloomLevels+2; postPoolInfo.poolSizeCount=1; postPoolInfo.pPoolSizes=&postSize;
    checked(vkCreateDescriptorPool(device,&postPoolInfo,nullptr,&postPool),"Post descriptor pool");
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; buffer.size=sizeof(FrameUniforms); buffer.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; buffer.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    checked(vkCreateBuffer(device,&buffer,nullptr,&frameBuffer),"Frame uniform buffer");
    VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,frameBuffer,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    checked(vkAllocateMemory(device,&allocation,nullptr,&frameMemory),"Frame uniform memory");
    checked(vkBindBufferMemory(device,frameBuffer,frameMemory,0),"Bind frame uniforms");
    checked(vkMapMemory(device,frameMemory,0,VK_WHOLE_SIZE,0,&frameMapped),"Map frame uniforms");
    VkDescriptorSetAllocateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; set.descriptorPool=framePool; set.descriptorSetCount=1; set.pSetLayouts=&frameLayout;
    checked(vkAllocateDescriptorSets(device,&set,&frameSet),"Frame descriptor set");
    VkDescriptorBufferInfo info{frameBuffer,0,sizeof(FrameUniforms)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=frameSet; write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; write.pBufferInfo=&info;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    checked(vkCreateSampler(device,&sampler,nullptr,&postSampler),"Post sampler");
    VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PostPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layoutInfo.setLayoutCount=1; layoutInfo.pSetLayouts=&postSetLayout;
    layoutInfo.pushConstantRangeCount=1; layoutInfo.pPushConstantRanges=&range;
    checked(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&postLayout),"Post pipeline layout");
}
void VulkanRenderer::writeFrameUniforms(const RenderFrame& frame,const glm::mat4& viewProjection) {
    auto front=forward(frame.camera.yaw,frame.camera.pitch);
    auto right=glm::normalize(glm::cross(front,glm::vec3(0,1,0)));
    const auto& e=frame.environment;
    FrameUniforms u{viewProjection,glm::vec4(frame.camera.position,1),glm::vec4(right,0),glm::vec4(glm::cross(right,front),0),
                    glm::vec4(e.background,e.fog),glm::vec4(e.ambient,e.sun,e.localLight,0)};
    // One frame in flight: the previous frame finished at the fence before recording.
    std::memcpy(frameMapped,&u,sizeof u);
}
void VulkanRenderer::createPostPipelines() {
    VkShaderModule vert=shader("post.vert.spv"),frag=VK_NULL_HANDLE;
    try {
        frag=shader("post.frag.spv");
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=1; viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo sample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; sample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
        auto build=[&](VkFormat target,bool additive,bool encode,VkPipeline& out) {
            VkPipelineColorBlendAttachmentState attachment{}; attachment.colorWriteMask=0xf;
            if(additive) {
                attachment.blendEnable=true;
                attachment.srcColorBlendFactor=attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE; attachment.colorBlendOp=VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO; attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE; attachment.alphaBlendOp=VK_BLEND_OP_ADD;
            }
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&attachment;
            VkBool32 encodeValue=encode;
            VkSpecializationMapEntry entry{0,0,sizeof(VkBool32)};
            VkSpecializationInfo specialization{1,&entry,sizeof(encodeValue),&encodeValue};
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};
            stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",&specialization};
            VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount=1; rendering.pColorAttachmentFormats=&target;
            VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext=&rendering; ci.stageCount=2; ci.pStages=stages;
            ci.pVertexInputState=&vertex; ci.pInputAssemblyState=&assembly; ci.pViewportState=&viewport; ci.pRasterizationState=&raster;
            ci.pMultisampleState=&sample; ci.pDepthStencilState=&ds; ci.pColorBlendState=&blend; ci.pDynamicState=&dynamic; ci.layout=postLayout;
            checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&out),"Post pipeline");
        };
        build(hdrFormat,false,false,postDown);
        build(hdrFormat,true,false,postUp);
        build(format,false,encodeSrgb,postComposite);
    } catch(...) { if(frag) vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr); throw; }
    vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr);
}
void VulkanRenderer::destroyImage(Image& image) {
    if(image.view) vkDestroyImageView(device,image.view,nullptr);
    if(image.image) vkDestroyImage(device,image.image,nullptr);
    if(image.memory) vkFreeMemory(device,image.memory,nullptr);
    image=Image{};
}
void VulkanRenderer::createPostTargets(VkExtent2D extent) {
    // Callers guarantee the previous targets are idle (frame fence).
    try {
        post.extent=extent;
        auto make=[&](VkExtent2D size,VkFormat imageFormat,VkImageUsageFlags usage,VkImageAspectFlags aspect) {
            Image image;image.extent=size;
            image.image=createImage(size,imageFormat,usage,aspect,image.memory,image.view);
            return image;
        };
        post.hdr=make(extent,hdrFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        post.depth=make(extent,depthFormat,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        VkExtent2D size{std::max(1u,extent.width/2),std::max(1u,extent.height/2)};
        while(post.bloom.size()<maxBloomLevels && (post.bloom.empty() || (size.width>=8 && size.height>=8))) {
            post.bloom.push_back(make(size,hdrFormat,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT));
            size={std::max(1u,size.width/2),std::max(1u,size.height/2)};
        }
        // Sets: prefilter(hdr), downsample(bloom[i-1]) for i>0, upsample(bloom[i]) for i>0, composite(hdr, bloom[0]).
        auto allocate=[&](VkImageView a,VkImageView b) {
            VkDescriptorSet set=VK_NULL_HANDLE;
            VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; info.descriptorPool=postPool; info.descriptorSetCount=1; info.pSetLayouts=&postSetLayout;
            checked(vkAllocateDescriptorSets(device,&info,&set),"Post descriptor set");
            post.sets.push_back(set);
            VkDescriptorImageInfo images[2]={{postSampler,a,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{postSampler,b,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=set; write.descriptorCount=2;
            write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo=images;
            vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        };
        allocate(post.hdr.view,post.hdr.view);
        for(size_t i=1;i<post.bloom.size();++i) allocate(post.bloom[i-1].view,post.bloom[i-1].view);
        for(size_t i=1;i<post.bloom.size();++i) allocate(post.bloom[i].view,post.bloom[i].view);
        allocate(post.hdr.view,post.bloom[0].view);
    } catch(...) {destroyPostTargets();throw;}
}
void VulkanRenderer::destroyPostTargets() {
    if(!post.sets.empty()) vkFreeDescriptorSets(device,postPool,uint32_t(post.sets.size()),post.sets.data());
    destroyImage(post.hdr);destroyImage(post.depth);
    for(auto& image:post.bloom) destroyImage(image);
    post=PostTargets{};
}
void VulkanRenderer::recordPost(const RenderFrame& frame,VkImageView output,VkExtent2D size) {
    const auto& environment=frame.environment;
    size_t levels=post.bloom.size();
    auto barrier=[&](VkImage image,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags from,VkPipelineStageFlags to) {
        auto b=transition(image,VK_IMAGE_ASPECT_COLOR_BIT,before,after,src,dst);
        vkCmdPipelineBarrier(command,from,to,0,0,nullptr,0,nullptr,1,&b);
    };
    auto pass=[&](VkImageView target,VkExtent2D extent,VkAttachmentLoadOp load,VkPipeline pipeline,VkDescriptorSet set,const PostPush& push) {
        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; color.imageView=target; color.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp=load; color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO}; info.renderArea={{0,0},extent}; info.layerCount=1; info.colorAttachmentCount=1; info.pColorAttachments=&color;
        vkCmdBeginRendering(command,&info);
        VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1}; VkRect2D scissor{{0,0},extent};
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,postLayout,0,1,&set,0,nullptr);
        vkCmdPushConstants(command,postLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PostPush),&push);
        vkCmdDraw(command,3,1,0,0);
        vkCmdEndRendering(command);
    };
    auto texel=[](VkExtent2D e){return glm::vec4(1.0f/float(e.width),1.0f/float(e.height),0,0);};
    glm::vec4 params(environment.bloomThreshold,environment.bloom,environment.exposure,1.0f/float(levels));
    int toneMap=environment.toneMap==ToneMap::Aces?1:0;
    constexpr auto attachment=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    constexpr auto fragment=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier(post.hdr.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,attachment,fragment);
    if(environment.bloom>0) {
        // Downsample chain: each level is written once, then sampled by the next.
        for(size_t i=0;i<levels;++i) {
            barrier(post.bloom[i].image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,fragment,attachment);
            PostPush push{texel(i==0?post.hdr.extent:post.bloom[i-1].extent),params,{i==0?0:1,toneMap,0,0}};
            pass(post.bloom[i].view,post.bloom[i].extent,VK_ATTACHMENT_LOAD_OP_DONT_CARE,postDown,post.sets[i],push);
            barrier(post.bloom[i].image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,attachment,fragment);
        }
        // Upsample: add each smaller level into the next larger one.
        for(size_t i=levels-1;i>=1;--i) {
            barrier(post.bloom[i-1].image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,fragment,attachment);
            PostPush push{texel(post.bloom[i].extent),params,{2,toneMap,0,0}};
            pass(post.bloom[i-1].view,post.bloom[i-1].extent,VK_ATTACHMENT_LOAD_OP_LOAD,postUp,post.sets[levels-1+i],push);
            barrier(post.bloom[i-1].image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,attachment,fragment);
        }
    } else {
        // The composite skips bloom sampling, but the descriptor still needs a valid layout.
        barrier(post.bloom[0].image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0,VK_ACCESS_SHADER_READ_BIT,fragment,fragment);
    }
    PostPush push{texel(size),params,{3,toneMap,0,0}};
    pass(output,size,VK_ATTACHMENT_LOAD_OP_DONT_CARE,postComposite,post.sets.back(),push);
}
}
