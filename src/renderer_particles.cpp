#include "renderer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
}
void VulkanRenderer::createParticlePipelines() {
    // Same layout as the scene pipeline (textures in set 0, frame uniforms in set 1).
    VkShaderModule vert=shader("particle.vert.spv"),frag=VK_NULL_HANDLE;
    try {
        frag=shader("particle.frag.spv");
        VkVertexInputBindingDescription binding{0,sizeof(ParticleVertex),VK_VERTEX_INPUT_RATE_INSTANCE};
        VkVertexInputAttributeDescription attributes[7]={
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(ParticleVertex,position)},
            {1,0,VK_FORMAT_R32_SFLOAT,offsetof(ParticleVertex,size)},
            {2,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(ParticleVertex,color)},
            {3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(ParticleVertex,velocity)},
            {4,0,VK_FORMAT_R32_SFLOAT,offsetof(ParticleVertex,rotation)},
            {5,0,VK_FORMAT_R32_SFLOAT,offsetof(ParticleVertex,stretch)},
            {6,0,VK_FORMAT_R32G32_UINT,offsetof(ParticleVertex,sprite)}};
        static_assert(offsetof(ParticleVertex,seed)==offsetof(ParticleVertex,sprite)+4);
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex.vertexBindingDescriptionCount=1; vertex.pVertexBindingDescriptions=&binding;
        vertex.vertexAttributeDescriptionCount=7; vertex.pVertexAttributeDescriptions=attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount=1; viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo sample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; sample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        // Particles test against scene depth but never write it, so they do not occlude each other.
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable=true; ds.depthWriteEnable=false; ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
        VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
        VkFormat target=hdrFormat;
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO}; rendering.colorAttachmentCount=1; rendering.pColorAttachmentFormats=&target; rendering.depthAttachmentFormat=depthFormat;
        for(bool additive:{true,false}) {
            // Premultiplied output: additive adds light, alpha composites over the scene.
            VkPipelineColorBlendAttachmentState attachment{};
            attachment.blendEnable=true; attachment.colorWriteMask=0xf;
            attachment.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
            attachment.dstColorBlendFactor=additive?VK_BLEND_FACTOR_ONE:VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp=VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO; attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE; attachment.alphaBlendOp=VK_BLEND_OP_ADD;
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&attachment;
            VkBool32 constant=additive;
            VkSpecializationMapEntry entry{0,0,sizeof(VkBool32)};
            VkSpecializationInfo specialization{1,&entry,sizeof(constant),&constant};
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};
            stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",&specialization};
            VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.pNext=&rendering; ci.stageCount=2; ci.pStages=stages;
            ci.pVertexInputState=&vertex; ci.pInputAssemblyState=&assembly; ci.pViewportState=&viewport; ci.pRasterizationState=&raster;
            ci.pMultisampleState=&sample; ci.pDepthStencilState=&ds; ci.pColorBlendState=&blend; ci.pDynamicState=&dynamic; ci.layout=layout;
            checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,additive?&particleAdditive:&particleAlpha),"Particle pipeline");
        }
    } catch(...) { if(frag) vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr); throw; }
    vkDestroyShaderModule(device,frag,nullptr); vkDestroyShaderModule(device,vert,nullptr);
}
void VulkanRenderer::reserveParticles(VkDeviceSize bytes) {
    if(bytes<=particleCapacity) return;
    // Called after the frame fence: the previous buffer is no longer in use.
    releaseParticles();
    VkDeviceSize capacity=std::max<VkDeviceSize>(bytes+bytes/2,64*1024);
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size=capacity; info.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    checked(vkCreateBuffer(device,&info,nullptr,&particleBuffer),"Particle buffer");
    VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,particleBuffer,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    try {
        checked(vkAllocateMemory(device,&allocation,nullptr,&particleMemory),"Particle memory");
        checked(vkBindBufferMemory(device,particleBuffer,particleMemory,0),"Bind particle memory");
        checked(vkMapMemory(device,particleMemory,0,VK_WHOLE_SIZE,0,&particleMapped),"Map particle memory");
    } catch(...) {releaseParticles();throw;}
    particleCapacity=capacity;
}
void VulkanRenderer::releaseParticles() {
    if(particleMapped) vkUnmapMemory(device,particleMemory);
    if(particleBuffer) vkDestroyBuffer(device,particleBuffer,nullptr);
    if(particleMemory) vkFreeMemory(device,particleMemory,nullptr);
    particleMapped=nullptr; particleBuffer=VK_NULL_HANDLE; particleMemory=VK_NULL_HANDLE; particleCapacity=0;
}
void VulkanRenderer::recordParticles(const RenderFrame& frame) {
    size_t total=0;
    for(const auto& batch:frame.particles) total+=batch.particles.size();
    if(!total) return;
    reserveParticles(VkDeviceSize(total)*sizeof(ParticleVertex));
    auto front=forward(frame.camera.yaw,frame.camera.pitch);
    // Alpha batches first (each sorted far to near), then additive light on top.
    std::vector<const ParticleBatch*> order;
    for(const auto& batch:frame.particles) if(batch.blend==ParticleBlend::Alpha) order.push_back(&batch);
    for(const auto& batch:frame.particles) if(batch.blend==ParticleBlend::Additive) order.push_back(&batch);
    auto* out=static_cast<ParticleVertex*>(particleMapped);
    struct Draw { const ParticleBatch* batch; uint32_t first,count; };
    std::vector<Draw> draws;
    uint32_t offset=0;
    std::vector<std::pair<float,uint32_t>> depth;
    for(const auto* batch:order) {
        auto count=uint32_t(batch->particles.size());
        if(!count) continue;
        if(batch->blend==ParticleBlend::Alpha) {
            depth.clear();
            for(uint32_t i=0;i<count;++i) depth.push_back({glm::dot(batch->particles[i].position-frame.camera.position,front),i});
            std::stable_sort(depth.begin(),depth.end(),[](const auto& a,const auto& b){return a.first>b.first;});
            for(uint32_t i=0;i<count;++i) out[offset+i]=batch->particles[depth[i].second];
        } else std::memcpy(out+offset,batch->particles.data(),size_t(count)*sizeof(ParticleVertex));
        draws.push_back({batch,offset,count});
        offset+=count;
    }
    VkDeviceSize zero=0;
    vkCmdBindVertexBuffers(command,0,1,&particleBuffer,&zero);
    for(const auto& draw:draws) {
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,draw.batch->blend==ParticleBlend::Additive?particleAdditive:particleAlpha);
        const auto& texture=gpuTextures.at(draw.batch->texture.get());
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&texture.descriptor,0,nullptr);
        vkCmdDraw(command,6,draw.count,0,draw.first);
        drawnParticles+=draw.count;
    }
}
}
