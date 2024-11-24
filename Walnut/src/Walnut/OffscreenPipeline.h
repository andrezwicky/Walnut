#pragma once
#include "vulkan/vulkan.h"

#include "Walnut/OffscreenImage.h"

namespace Walnut
{
    class OffscreenPipeline
    {
    public:
        OffscreenPipeline(VkQueue queue, OffscreenImage& image);
        ~OffscreenPipeline();

        void UploadDrawData(ImDrawData* drawData);
        void RecordCommandBuffer(ImDrawData* drawData);
        void Submit();


    private:
        VkQueue queue;

        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        VkShaderModule m_VertShaderModule = VK_NULL_HANDLE;
        VkShaderModule m_FragShaderModule = VK_NULL_HANDLE;

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize currentVertexBufferSize = 0;

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize currentIndexBufferSize = 0;

        OffscreenImage& offscreenImage; // Reference to the offscreen image

        void CreateRenderPass();
        void CreateFramebuffer();
        void CreatePipeline();
        void AllocateCommandBuffer();

        void CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize& currentSize, VkDeviceSize newSize, VkBufferUsageFlags usage);
    };
}

