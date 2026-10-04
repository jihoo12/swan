#include "renderer.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace swan {
namespace {
void checked(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (VkResult "+std::to_string(result)+")");
}
VkBufferMemoryBarrier access(VkBuffer buffer,VkAccessFlags source,VkAccessFlags destination) {
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask=source; barrier.dstAccessMask=destination;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=buffer; barrier.size=VK_WHOLE_SIZE; return barrier;
}
}
VulkanRenderer::GpuMesh VulkanRenderer::uploadMesh(SharedMesh data) {
    if(!data || data->vertices.empty() || data->indices.empty()) throw std::invalid_argument("Cannot upload an empty mesh");
    if(data->indices.size()>std::numeric_limits<uint32_t>::max() ||
       data->vertices.size()>std::numeric_limits<VkDeviceSize>::max()/sizeof(Vertex) ||
       data->indices.size()>(std::numeric_limits<VkDeviceSize>::max()-data->vertices.size()*sizeof(Vertex))/sizeof(uint32_t))
        throw std::invalid_argument("Mesh exceeds GPU buffer limits");
    GpuMesh mesh; mesh.owner=std::move(data);
    mesh.indexOffset=mesh.owner->vertices.size()*sizeof(Vertex);
    mesh.byteSize=mesh.indexOffset+mesh.owner->indices.size()*sizeof(uint32_t);
    mesh.indexCount=static_cast<uint32_t>(mesh.owner->indices.size());
    VkBuffer staging=VK_NULL_HANDLE,readback=VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory=VK_NULL_HANDLE,readbackMemory=VK_NULL_HANDLE;
    VkCommandBuffer command=VK_NULL_HANDLE; bool submitted=false;
    auto cleanup=[&] {
        if(submitted) vkQueueWaitIdle(queue);
        if(command) vkFreeCommandBuffers(device,pool,1,&command);
        if(staging) vkDestroyBuffer(device,staging,nullptr);
        if(readback) vkDestroyBuffer(device,readback,nullptr);
        if(stagingMemory) vkFreeMemory(device,stagingMemory,nullptr);
        if(readbackMemory) vkFreeMemory(device,readbackMemory,nullptr);
    };
    auto allocate=[&](VkBufferUsageFlags usage,VkMemoryPropertyFlags properties,VkBuffer& buffer,VkDeviceMemory& memory) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=mesh.byteSize; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&info,nullptr,&buffer),"Create mesh upload buffer");
        VkMemoryRequirements requirements; vkGetBufferMemoryRequirements(device,buffer,&requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=requirements.size; allocation.memoryTypeIndex=memoryType(requirements.memoryTypeBits,properties);
        checked(vkAllocateMemory(device,&allocation,nullptr,&memory),"Allocate mesh upload memory");
        checked(vkBindBufferMemory(device,buffer,memory,0),"Bind mesh upload buffer");
    };
    try {
        allocate(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,staging,stagingMemory);
        allocate(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|
                 (options.verifyMeshUploads?VK_BUFFER_USAGE_TRANSFER_SRC_BIT:0),VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,mesh.buffer,mesh.memory);
        if(options.verifyMeshUploads) allocate(VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,readback,readbackMemory);
        void* mapped=nullptr;
        checked(vkMapMemory(device,stagingMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map mesh staging memory");
        std::memcpy(mapped,mesh.owner->vertices.data(),size_t(mesh.indexOffset));
        std::memcpy(static_cast<char*>(mapped)+mesh.indexOffset,mesh.owner->indices.data(),size_t(mesh.byteSize-mesh.indexOffset));
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=stagingMemory; range.size=VK_WHOLE_SIZE;
        auto result=vkFlushMappedMemoryRanges(device,1,&range); vkUnmapMemory(device,stagingMemory);
        checked(result,"Flush mesh staging memory");
        VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        commands.commandPool=pool; commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; commands.commandBufferCount=1;
        checked(vkAllocateCommandBuffers(device,&commands,&command),"Mesh upload command");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        checked(vkBeginCommandBuffer(command,&begin),"Begin mesh upload");
        VkBufferCopy copy{0,0,mesh.byteSize}; vkCmdCopyBuffer(command,staging,mesh.buffer,1,&copy);
        if(options.verifyMeshUploads) {
            auto toReadback=access(mesh.buffer,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&toReadback,0,nullptr);
            vkCmdCopyBuffer(command,mesh.buffer,readback,1,&copy);
            auto toHost=access(readback,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT);
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&toHost,0,nullptr);
        }
        auto toDraw=access(mesh.buffer,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT|VK_ACCESS_INDEX_READ_BIT);
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&toDraw,0,nullptr);
        checked(vkEndCommandBuffer(command),"End mesh upload");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
        checked(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit mesh upload"); submitted=true;
        checked(vkQueueWaitIdle(queue),"Wait mesh upload"); submitted=false;
        if(options.verifyMeshUploads) {
            checked(vkMapMemory(device,readbackMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map mesh readback");
            range.memory=readbackMemory;
            auto invalidated=vkInvalidateMappedMemoryRanges(device,1,&range);
            bool identical=false;
            if(invalidated==VK_SUCCESS) identical=std::memcmp(mapped,mesh.owner->vertices.data(),size_t(mesh.indexOffset))==0 &&
                std::memcmp(static_cast<char*>(mapped)+mesh.indexOffset,mesh.owner->indices.data(),size_t(mesh.byteSize-mesh.indexOffset))==0;
            vkUnmapMemory(device,readbackMemory);
            checked(invalidated,"Invalidate mesh readback");
            if(!identical) throw std::runtime_error("GPU mesh readback differs from CPU vertices/indices");
            ++verifiedMeshUploads;
        }
    } catch(...) {cleanup(); releaseMesh(mesh); throw;}
    cleanup(); return mesh;
}
}
