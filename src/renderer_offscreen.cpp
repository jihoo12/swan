#include "renderer.hpp"
#include <cstring>
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
VkImageMemoryBarrier imageBarrier(VkImage image,VkImageAspectFlags aspect,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask=src; b.dstAccessMask=dst; b.oldLayout=before; b.newLayout=after;
    b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.image=image; b.subresourceRange={aspect,0,1,0,1}; return b;
}
}
void VulkanRenderer::releaseReadback() {
    if(readback) vkDestroyBuffer(device,readback,nullptr);
    if(readbackMemory) vkFreeMemory(device,readbackMemory,nullptr);
    readback=VK_NULL_HANDLE; readbackMemory=VK_NULL_HANDLE; readbackCapacity=0;
}
TextureData VulkanRenderer::renderImage(const RenderFrame& frame,glm::uvec2 size) {
    if(!options.headless) throw std::logic_error("renderImage() needs a renderer created with Options::headless");
    if(size.x==0 || size.y==0 || size.x>8192 || size.y>8192) throw std::invalid_argument("Render size must be within 1..8192 pixels");
    checked(vkWaitForFences(device,1,&fence,true,UINT64_MAX),"Wait previous render");
    if(sceneTarget.extent.width!=size.x || sceneTarget.extent.height!=size.y) {destroySceneTarget();createSceneTarget({size.x,size.y});}
    synchronizeMeshes(frame);
    synchronizeTextures(frame);
    VkDeviceSize bytes=VkDeviceSize(size.x)*size.y*4;
    if(bytes>readbackCapacity) {
        releaseReadback();
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size=bytes; info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&info,nullptr,&readback),"Readback buffer");
        VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,readback,&requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        try {
            checked(vkAllocateMemory(device,&allocation,nullptr,&readbackMemory),"Readback memory");
            checked(vkBindBufferMemory(device,readback,readbackMemory,0),"Bind readback memory");
        } catch(...) {releaseReadback();throw;}
        readbackCapacity=bytes;
    }
    checked(vkResetCommandBuffer(command,0),"Reset command");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checked(vkBeginCommandBuffer(command,&begin),"Begin offscreen render");
    // The previous render's copy finished at the fence; discard the image.
    auto color=imageBarrier(sceneTarget.color,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&color);
    recordScene(frame,sceneTarget.colorView,sceneTarget.extent);
    auto toCopy=imageBarrier(sceneTarget.color,VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&toCopy);
    VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={size.x,size.y,1};
    vkCmdCopyImageToBuffer(command,sceneTarget.color,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
    VkBufferMemoryBarrier toHost{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    toHost.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; toHost.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    toHost.srcQueueFamilyIndex=toHost.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; toHost.buffer=readback; toHost.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&toHost,0,nullptr);
    checked(vkEndCommandBuffer(command),"End offscreen render");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
    checked(vkResetFences(device,1,&fence),"Reset render fence");
    checked(vkQueueSubmit(queue,1,&submit,fence),"Submit offscreen render");
    checked(vkWaitForFences(device,1,&fence,true,UINT64_MAX),"Wait offscreen render");
    ++renderedFrames; submittedObjects+=frameVisible; culledObjects+=frameCulled;
    TextureData image{size.x,size.y,std::vector<uint8_t>(size_t(bytes))};
    void* mapped=nullptr;
    checked(vkMapMemory(device,readbackMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map readback");
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=readbackMemory; range.size=VK_WHOLE_SIZE;
    auto invalidated=vkInvalidateMappedMemoryRanges(device,1,&range);
    if(invalidated==VK_SUCCESS) std::memcpy(image.rgba.data(),mapped,size_t(bytes));
    vkUnmapMemory(device,readbackMemory);
    checked(invalidated,"Invalidate readback");
    for(size_t i=3;i<image.rgba.size();i+=4) image.rgba[i]=255; // Opaque output; blending used alpha only.
    return image;
}
}
