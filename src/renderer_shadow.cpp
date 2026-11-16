#include "renderer.hpp"
#include "visibility.hpp"
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
struct ShadowPush { glm::vec4 positionUvV,scaleGlow,color,extra; };
static_assert(sizeof(ShadowPush)==64);
}
void VulkanRenderer::createShadowResources() {
    // A depth format that can be rendered and sampled with depth comparison.
    VkFormatFeatureFlags needed=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    bool linear=false;
    for(auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM}) {
        VkFormatProperties p; vkGetPhysicalDeviceFormatProperties(gpu,candidate,&p);
        if((p.optimalTilingFeatures&needed)==needed) {
            shadowFormat=candidate; linear=p.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT; break;
        }
    }
    if(shadowFormat==VK_FORMAT_UNDEFINED) throw std::runtime_error("No sampleable depth format for shadow maps");
    shadowMap.extent={shadowResolution,shadowResolution};
    shadowMap.image=createImage(shadowMap.extent,shadowFormat,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_IMAGE_ASPECT_DEPTH_BIT,shadowMap.memory,shadowMap.view);
    // Comparison sampling; outside the map counts as lit.
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=linear?VK_FILTER_LINEAR:VK_FILTER_NEAREST; sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler.compareEnable=true; sampler.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
    checked(vkCreateSampler(device,&sampler,nullptr,&shadowSampler),"Shadow sampler");
}
void VulkanRenderer::destroyShadowResources() {
    destroyImage(shadowMap);
    if(shadowSampler) vkDestroySampler(device,shadowSampler,nullptr);
    shadowSampler=VK_NULL_HANDLE;
}
void VulkanRenderer::createShadowPipeline() {
    VkShaderModule vert=shader("shadow.vert.spv");
    try {
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};
        VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription position{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)};
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex.vertexBindingDescriptionCount=1; vertex.pVertexBindingDescriptions=&binding;
        vertex.vertexAttributeDescriptionCount=1; vertex.pVertexAttributeDescriptions=&position;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=1; viewport.scissorCount=1;
        // Slope-scaled bias against acne; the fragment shader adds a normal offset.
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
        raster.depthBiasEnable=true; raster.depthBiasConstantFactor=1.25f; raster.depthBiasSlopeFactor=1.75f;
        VkPipelineMultisampleStateCreateInfo sample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; sample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable=true; ds.depthWriteEnable=true; ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.depthAttachmentFormat=shadowFormat;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext=&rendering; ci.stageCount=1; ci.pStages=&stage;
        ci.pVertexInputState=&vertex; ci.pInputAssemblyState=&assembly; ci.pViewportState=&viewport; ci.pRasterizationState=&raster;
        ci.pMultisampleState=&sample; ci.pDepthStencilState=&ds; ci.pColorBlendState=&blend; ci.pDynamicState=&dynamic; ci.layout=layout;
        checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&shadowPipeline),"Shadow pipeline");
    } catch(...) {vkDestroyShaderModule(device,vert,nullptr);throw;}
    vkDestroyShaderModule(device,vert,nullptr);
}
void VulkanRenderer::recordShadows(const RenderFrame& frame,const glm::mat4& shadowViewProjection) {
    // The previous frame's sampling finished at the fence; discard and redraw the map.
    auto toDepth=VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toDepth.srcAccessMask=0; toDepth.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toDepth.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; toDepth.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    toDepth.srcQueueFamilyIndex=toDepth.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    toDepth.image=shadowMap.image; toDepth.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0,0,nullptr,0,nullptr,1,&toDepth);
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO}; depth.imageView=shadowMap.view; depth.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; depth.storeOp=VK_ATTACHMENT_STORE_OP_STORE; depth.clearValue.depthStencil={1,0};
    VkRenderingInfo render{VK_STRUCTURE_TYPE_RENDERING_INFO}; render.renderArea={{0,0},shadowMap.extent}; render.layerCount=1; render.pDepthAttachment=&depth;
    vkCmdBeginRendering(command,&render);
    shadowCasters=0;
    // Disabled shadows still clear the map so the scene pass samples a valid image.
    if(frame.environment.shadows>0) {
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowPipeline);
        VkViewport viewport{0,0,float(shadowMap.extent.width),float(shadowMap.extent.height),0,1}; VkRect2D scissor{{0,0},shadowMap.extent};
        vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,1,1,&frameSet,0,nullptr);
        Frustum volume(shadowViewProjection);
        for(const auto& object:frame.objects) {
            if(options.culling && !volume.intersects(*object.mesh,object.transform)) continue;
            ++shadowCasters;
            ShadowPush push{glm::vec4(object.transform.position,0),glm::vec4(object.transform.scale,0),glm::vec4(0,0,0,object.transform.yaw),glm::vec4(0)};
            vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
            const auto& mesh=gpuMeshes.at(object.mesh.get());
            VkDeviceSize offset=0;
            vkCmdBindVertexBuffers(command,0,1,&mesh.buffer,&offset);
            vkCmdBindIndexBuffer(command,mesh.buffer,mesh.indexOffset,VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command,mesh.indexCount,1,0,0,0);
        }
    }
    vkCmdEndRendering(command);
    auto toRead=toDepth;
    toRead.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; toRead.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    toRead.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; toRead.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,0,nullptr,0,nullptr,1,&toRead);
}
}
