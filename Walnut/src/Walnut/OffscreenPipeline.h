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
        VkQueue                 m_Queue;

        VkRenderPass            m_RenderPass;
        VkFramebuffer           m_FrameBuffer;
        VkPipeline              m_PipeLine;
        VkPipelineLayout        m_PipeLineLayout;
        VkCommandPool           m_CommandPool;
        VkCommandBuffer         m_CommandBuffer;

        VkShaderModule          m_VertShaderModule = VK_NULL_HANDLE;
        VkShaderModule          m_FragShaderModule = VK_NULL_HANDLE;

        VkDescriptorSetLayout   m_DescriptorSetLayout = VK_NULL_HANDLE;

        VkBuffer                m_VertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory          m_VertexBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize            m_CurrentVertexBufferSize = 0;

        VkBuffer                m_IndexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory          m_IndexBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize            m_CurrentIndexBufferSize = 0;

        OffscreenImage&         m_OffscreenImage; // Reference to the offscreen image

        void CreateRenderPass();
        void CreateFramebuffer();
        void CreatePipeline();
        void AllocateCommandBuffer();

        void CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize& currentSize, VkDeviceSize newSize, VkBufferUsageFlags usage);
    };
}

