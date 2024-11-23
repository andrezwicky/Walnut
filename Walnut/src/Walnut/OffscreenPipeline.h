#pragma once
#include "vulkan/vulkan.h"

#include "Walnut/OffscreenImage.h"

namespace Walnut
{
    class OffscreenPipeline {
    public:
        OffscreenPipeline(VkQueue queue, OffscreenImage& image);
        ~OffscreenPipeline();

        void RecordCommandBuffer();
        void Submit();

    private:
        VkQueue queue;

        VkRenderPass renderPass;
        VkFramebuffer framebuffer;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        OffscreenImage& offscreenImage; // Reference to the offscreen image

        void CreateRenderPass();
        void CreateFramebuffer();
        void CreatePipeline();
        void AllocateCommandBuffer();
    };
}

