#include "Walnut/Offscreenpipeline.h"
#include "Walnut/Utils.h"
#include <backends/imgui_impl_vulkan.h>

#include "imgui.h"
#include <array>

namespace Walnut
{
    
    OffscreenPipeline::OffscreenPipeline(VkQueue queue, OffscreenImage& image)
        : queue(queue), offscreenImage(image)
    {
        CreateRenderPass();
        CreateFramebuffer();
        CreatePipeline();
        AllocateCommandBuffer();
    }
    OffscreenPipeline::~OffscreenPipeline()
    {
        VkDevice device = Walnut::Application::GetDevice();

        vkDestroyFramebuffer(device, framebuffer, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);

        // Cleanup descriptor set layout after pipeline creation
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);

        if (m_VertShaderModule)
        {
            vkDestroyShaderModule(device, m_VertShaderModule, nullptr);
            m_VertShaderModule = VK_NULL_HANDLE;
        }

        if (m_FragShaderModule)
        {
            vkDestroyShaderModule(device, m_FragShaderModule, nullptr);
            m_FragShaderModule = VK_NULL_HANDLE;
        }
    }
    void OffscreenPipeline::CreateRenderPass()
    {
        VkDevice device = Walnut::Application::GetDevice();

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = offscreenImage.GetVkImageFormat(); // Use the format of the OffscreenImage
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    }
    void OffscreenPipeline::CreateFramebuffer()
    {
        VkDevice device = Walnut::Application::GetDevice();

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &offscreenImage.GetImageView();
        framebufferInfo.width = offscreenImage.GetWidth();
        framebufferInfo.height = offscreenImage.GetHeight();
        framebufferInfo.layers = 1;

        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
    }
    void OffscreenPipeline::CreatePipeline()
    {
        VkDevice device = Walnut::Application::GetDevice();

        // Define the Descriptor Set Layout (Step 1)
        VkDescriptorSetLayoutBinding samplerBinding = {};
        samplerBinding.binding = 0;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = 1;
        descriptorSetLayoutInfo.pBindings = &samplerBinding;

        if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout!");

        // Push Constant Range (if used)
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(float) * 4;

        // Create Pipeline Layout (Step 2)
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout!");

        size_t vertShaderSize = 0;
        const uint32_t* vertShaderCode = GetImGuiVertexShader(&vertShaderSize);

        size_t fragShaderSize = 0;
        const uint32_t* fragShaderCode = GetImGuiFragmentShader(&fragShaderSize);

        // === Shader Module Creation ===
        VkShaderModuleCreateInfo vertInfo = {};
        vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertInfo.codeSize = vertShaderSize;
        vertInfo.pCode = vertShaderCode;
        if (vkCreateShaderModule(device, &vertInfo, nullptr, &m_VertShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex shader module!");
        }

        VkShaderModuleCreateInfo fragInfo = {};
        fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragInfo.codeSize = fragShaderSize;
        fragInfo.pCode = fragShaderCode;
        if (vkCreateShaderModule(device, &fragInfo, nullptr, &m_FragShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fragment shader module!");
        }

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = m_VertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = m_FragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // === Vertex Input State ===
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(ImDrawVert);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

        // Position attribute (location 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(ImDrawVert, pos);

        // UV attribute (location 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(ImDrawVert, uv);

        // Color attribute (location 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
        attributeDescriptions[2].offset = offsetof(ImDrawVert, col);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // === Input Assembly State ===
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // === Viewport State ===
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)offscreenImage.GetWidth();
        viewport.height = (float)offscreenImage.GetHeight();
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { offscreenImage.GetWidth(), offscreenImage.GetHeight() };

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // === Dynamic State ===
        std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // === Rasterization State ===
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // === Multisample State ===
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // === Color Blend State ===
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // === Create Pipeline ===
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState; // Enable dynamic state here
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
    }



    void OffscreenPipeline::AllocateCommandBuffer() {
        VkDevice device = Walnut::Application::GetDevice();
        VkPhysicalDevice physicalDevice = Application::GetPhysicalDevice();

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = Utils::GetGraphicsQueueFamilyIndex(physicalDevice);
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    }
    void OffscreenPipeline::UploadDrawData(ImDrawData* drawData)
    {
        auto const& device = Application::GetDevice();
        if (!drawData || drawData->TotalVtxCount == 0) return;

        // Calculate required buffer sizes
        VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

        // Create or resize vertex buffer
        if (vertexBuffer == VK_NULL_HANDLE || vertexBufferSize > currentVertexBufferSize) {
            CreateOrResizeBuffer(vertexBuffer, vertexBufferMemory, currentVertexBufferSize, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }

        // Create or resize index buffer
        if (indexBuffer == VK_NULL_HANDLE || indexBufferSize > currentIndexBufferSize) {
            CreateOrResizeBuffer(indexBuffer, indexBufferMemory, currentIndexBufferSize, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }

        // Upload vertex and index data
        ImDrawVert* vtxDst = nullptr;
        ImDrawIdx* idxDst = nullptr;

        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, (void**)&vtxDst);
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, (void**)&idxDst);

        for (int n = 0; n < drawData->CmdListsCount; n++) {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        vkUnmapMemory(device, vertexBufferMemory);
        vkUnmapMemory(device, indexBufferMemory);
    }


    void OffscreenPipeline::CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize& currentSize, VkDeviceSize newSize, VkBufferUsageFlags usage)
    {
        auto const& device = Application::GetDevice();
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
        if (bufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, bufferMemory, nullptr);
        }

        // Create new buffer
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = newSize;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        // Allocate memory for the buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate buffer memory!");

        vkBindBufferMemory(device, buffer, bufferMemory, 0);

        // Update current size
        currentSize = newSize;
    }


    void OffscreenPipeline::RecordCommandBuffer(ImDrawData* drawData) {
        // Initialize command buffer begin info
        VkCommandBufferBeginInfo commandBufferBeginInfo = {};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        // Initialize render pass begin info
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffer;
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = { offscreenImage.GetWidth(), offscreenImage.GetHeight() };

        VkClearValue clearValue = {};
        clearValue.color = { 1.0f, 0.0f, 1.0f, 1.0f };
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Dynamically set the viewport
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(offscreenImage.GetWidth());
        viewport.height = static_cast<float>(offscreenImage.GetHeight());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        // Push constants for the vertex shader
        struct PushConstants {
            float offset[2];
            float scale[2];
        };

        PushConstants pushConstants = {
            {drawData->DisplayPos.x, drawData->DisplayPos.y}, // Offset
            {1.0f / drawData->DisplaySize.x, 1.0f / drawData->DisplaySize.y} // Scale
        };

        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConstants),
            &pushConstants
        );

        // Bind vertex and index buffers
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // Render ImGui draw commands
        int vertexOffset = 0;
        int indexOffset = 0;

        for (int n = 0; n < drawData->CmdListsCount; n++) {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];

                // Apply scissor
                VkRect2D scissor;
                scissor.offset.x = std::max((int32_t)pcmd->ClipRect.x, 0);
                scissor.offset.y = std::max((int32_t)pcmd->ClipRect.y, 0);
                scissor.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissor.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                // Bind texture (if used)
                VkDescriptorSet descSet = (VkDescriptorSet)pcmd->GetTexID();
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

                // Draw indexed
                vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            }
            indexOffset += cmdList->IdxBuffer.Size;
            vertexOffset += cmdList->VtxBuffer.Size;
        }

        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
    }




    void OffscreenPipeline::Submit()
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
    }



}