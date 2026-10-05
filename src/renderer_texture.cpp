#include "renderer.hpp"
#include <cstring>
#include <set>
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
VkImageMemoryBarrier transition(VkImage image,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,uint32_t levels) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image=image; barrier.oldLayout=before; barrier.newLayout=after;
    barrier.srcAccessMask=src; barrier.dstAccessMask=dst;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,1}; return barrier;
}
}
void VulkanRenderer::createDescriptors() {
    VkFormatProperties properties; vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_R8G8B8A8_SRGB,&properties);
    VkFormatFeatureFlags required=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if((properties.optimalTilingFeatures&required)!=required) throw std::runtime_error("GPU lacks RGBA8 sRGB texture support");
    VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount=1; layoutInfo.pBindings=&binding;
    checked(vkCreateDescriptorSetLayout(device,&layoutInfo,nullptr,&textureLayout),"Texture descriptor layout");
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; poolInfo.maxSets=1024;
    poolInfo.poolSizeCount=1; poolInfo.pPoolSizes=&size;
    checked(vkCreateDescriptorPool(device,&poolInfo,nullptr,&texturePool),"Texture descriptor pool");
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.maxLod=VK_LOD_CLAMP_NONE;
    checked(vkCreateSampler(device,&sampler,nullptr,&textureSampler),"Texture sampler");
}
VulkanRenderer::GpuTexture VulkanRenderer::uploadTexture(SharedTexture data) {
    if(!data || !data->width || !data->height || data->width>4096 || data->height>4096 ||
       data->rgba.size()!=size_t(data->width)*data->height*4) throw std::invalid_argument("Invalid texture pixels/dimensions");
    auto levels=buildMipChain(*data);
    std::vector<uint8_t> pixels;
    std::vector<VkBufferImageCopy> copies;
    for (uint32_t level=0;level<levels.size();++level) {
        VkBufferImageCopy copy{}; copy.bufferOffset=pixels.size();
        copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,1};
        copy.imageExtent={levels[level].width,levels[level].height,1};
        copies.push_back(copy);
        pixels.insert(pixels.end(),levels[level].rgba.begin(),levels[level].rgba.end());
    }
    uint32_t mipLevels=static_cast<uint32_t>(levels.size());
    GpuTexture texture; texture.owner=std::move(data);
    VkBuffer staging=VK_NULL_HANDLE; VkDeviceMemory stagingMemory=VK_NULL_HANDLE;
    VkCommandBuffer upload=VK_NULL_HANDLE; bool submitted=false;
    auto cleanupUpload=[&] {
        if(submitted) vkQueueWaitIdle(queue);
        if(upload) vkFreeCommandBuffers(device,pool,1,&upload);
        if(staging) vkDestroyBuffer(device,staging,nullptr);
        if(stagingMemory) vkFreeMemory(device,stagingMemory,nullptr);
    };
    try {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size=pixels.size(); bufferInfo.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&bufferInfo,nullptr,&staging),"Texture staging buffer");
        VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,staging,&requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        checked(vkAllocateMemory(device,&allocation,nullptr,&stagingMemory),"Texture staging memory");
        checked(vkBindBufferMemory(device,staging,stagingMemory,0),"Bind texture staging memory");
        void* mapped=nullptr; checked(vkMapMemory(device,stagingMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map texture pixels");
        std::memcpy(mapped,pixels.data(),pixels.size());
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=stagingMemory; range.size=VK_WHOLE_SIZE;
        auto result=vkFlushMappedMemoryRanges(device,1,&range); vkUnmapMemory(device,stagingMemory);
        checked(result,"Flush texture pixels");
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; image.imageType=VK_IMAGE_TYPE_2D;
        image.format=VK_FORMAT_R8G8B8A8_SRGB; image.extent={texture.owner->width,texture.owner->height,1};
        image.mipLevels=mipLevels; image.arrayLayers=1; image.samples=VK_SAMPLE_COUNT_1_BIT; image.tiling=VK_IMAGE_TILING_OPTIMAL;
        image.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        checked(vkCreateImage(device,&image,nullptr,&texture.image),"Texture image");
        vkGetImageMemoryRequirements(device,texture.image,&requirements);
        allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        checked(vkAllocateMemory(device,&allocation,nullptr,&texture.memory),"Texture image memory");
        checked(vkBindImageMemory(device,texture.image,texture.memory,0),"Bind texture image");
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        commandInfo.commandPool=pool; commandInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; commandInfo.commandBufferCount=1;
        checked(vkAllocateCommandBuffers(device,&commandInfo,&upload),"Texture upload command");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        checked(vkBeginCommandBuffer(upload,&begin),"Begin texture upload");
        auto toTransfer=transition(texture.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,mipLevels);
        vkCmdPipelineBarrier(upload,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&toTransfer);
        vkCmdCopyBufferToImage(upload,staging,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(copies.size()),copies.data());
        auto toSample=transition(texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,mipLevels);
        vkCmdPipelineBarrier(upload,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&toSample);
        checked(vkEndCommandBuffer(upload),"End texture upload");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&upload;
        checked(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit texture upload"); submitted=true;
        checked(vkQueueWaitIdle(queue),"Wait texture upload"); submitted=false;
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image=texture.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view.format=image.format; view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,mipLevels,0,1};
        checked(vkCreateImageView(device,&view,nullptr,&texture.view),"Texture view");
        VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        descriptor.descriptorPool=texturePool; descriptor.descriptorSetCount=1; descriptor.pSetLayouts=&textureLayout;
        checked(vkAllocateDescriptorSets(device,&descriptor,&texture.descriptor),"Texture descriptor");
        VkDescriptorImageInfo imageInfo{textureSampler,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet=texture.descriptor; write.dstBinding=0;
        write.descriptorCount=1; write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo=&imageInfo;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    } catch(...) { cleanupUpload(); releaseTexture(texture); throw; }
    cleanupUpload(); return texture;
}
void VulkanRenderer::releaseTexture(GpuTexture& texture) {
    if(texture.descriptor) vkFreeDescriptorSets(device,texturePool,1,&texture.descriptor);
    if(texture.view) vkDestroyImageView(device,texture.view,nullptr);
    if(texture.image) vkDestroyImage(device,texture.image,nullptr);
    if(texture.memory) vkFreeMemory(device,texture.memory,nullptr);
    texture.descriptor=VK_NULL_HANDLE; texture.view=VK_NULL_HANDLE; texture.image=VK_NULL_HANDLE; texture.memory=VK_NULL_HANDLE;
}
void VulkanRenderer::synchronizeTextures(const RenderFrame& frame) {
    std::set<const TextureData*> needed;
    std::vector<SharedTexture> used;
    for(const auto& object:frame.objects) {
        if(!object.texture) throw std::invalid_argument("Render object has no texture");
        if(needed.insert(object.texture.get()).second) used.push_back(object.texture);
    }
    for(const auto& batch:frame.particles) {
        if(!batch.texture) throw std::invalid_argument("Particle batch has no texture");
        if(needed.insert(batch.texture.get()).second) used.push_back(batch.texture);
    }
    if(needed.size()>1024) throw std::invalid_argument("GPU texture cache exceeds 1024 resources");
    for(auto it=gpuTextures.begin();it!=gpuTextures.end();) {
        if(!needed.contains(it->first)) { releaseTexture(it->second); it=gpuTextures.erase(it); }
        else ++it;
    }
    for(const auto& shared:used) if(!gpuTextures.contains(shared.get())) {
        auto texture=uploadTexture(shared);
        try {gpuTextures.emplace(shared.get(),texture); ++uploadedTextures;}
        catch(...) {releaseTexture(texture); throw;}
    }
}
}
