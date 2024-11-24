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

        std::cout << "Debug: Creating render pass" << std::endl;

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = offscreenImage.GetVkImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear buffer at start
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        std::cout << "Debug: Color attachment format: " << colorAttachment.format << std::endl;


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

        std::cout << "Debug: Render pass created successfully" << std::endl;
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
        auto const& device = Application::GetDevice();
        std::cout << "Debug: Creating pipeline with image dimensions: "
            << offscreenImage.GetWidth() << "x" << offscreenImage.GetHeight() << std::endl;

        // Debug print shader code sizes
        size_t vertSize = 0, fragSize = 0;
        const uint32_t* vertCode = GetImGuiVertexShader(&vertSize);
        const uint32_t* fragCode = GetImGuiFragmentShader(&fragSize);
        std::cout << "Debug: Vertex shader size: " << vertSize << ", Fragment shader size: " << fragSize << std::endl;

        // Create shader modules with verification
        VkShaderModuleCreateInfo vertInfo = {};
        vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertInfo.codeSize = vertSize;
        vertInfo.pCode = vertCode;

        VkShaderModuleCreateInfo fragInfo = {};
        fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragInfo.codeSize = fragSize;
        fragInfo.pCode = fragCode;

        VkResult err;
        err = vkCreateShaderModule(device, &vertInfo, nullptr, &m_VertShaderModule);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create vertex shader module" << std::endl;
            return;
        }

        err = vkCreateShaderModule(device, &fragInfo, nullptr, &m_FragShaderModule);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create fragment shader module" << std::endl;
            return;
        }

        std::cout << "Debug: Shader modules created successfully" << std::endl;

        // Create Descriptor Set Layout
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        err = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create descriptor set layout" << std::endl;
            return;
        }

        // Create Pipeline Layout with Push Constants
        VkPushConstantRange pushConstant = {};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(float) * 4;  // 2 vec2s: scale and translate

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

        err = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create pipeline layout" << std::endl;
            return;
        }

        std::cout << "Debug: Pipeline layout created successfully" << std::endl;

        // Create Pipeline
        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = m_VertShaderModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = m_FragShaderModule;
        stages[1].pName = "main";

        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(ImDrawVert);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDesc[3] = {};
        attributeDesc[0].location = 0;
        attributeDesc[0].binding = binding.binding;
        attributeDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDesc[0].offset = offsetof(ImDrawVert, pos);

        attributeDesc[1].location = 1;
        attributeDesc[1].binding = binding.binding;
        attributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDesc[1].offset = offsetof(ImDrawVert, uv);

        attributeDesc[2].location = 2;
        attributeDesc[2].binding = binding.binding;
        attributeDesc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
        attributeDesc[2].offset = offsetof(ImDrawVert, col);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor are dynamic
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create graphics pipeline" << std::endl;
            return;
        }

        std::cout << "Debug: Pipeline created successfully" << std::endl;
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
        // Validate input data
        std::cout << "Debug: Starting UploadDrawData" << std::endl;
        if (!drawData || drawData->TotalVtxCount == 0)
        {
            std::cout << "Error: Invalid draw data" << std::endl;
            return;
        }

        VkDevice device = Application::GetDevice();

        // Calculate buffer sizes
        VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

        std::cout << "Debug: Creating buffers - Vertex size: " << vertexBufferSize
            << ", Index size: " << indexBufferSize << std::endl;

        // Create or resize vertex buffer
        if (vertexBuffer == VK_NULL_HANDLE || currentVertexBufferSize < vertexBufferSize)
        {
            if (vertexBuffer != VK_NULL_HANDLE)
            {
                std::cout << "Debug: Recreating vertex buffer" << std::endl;
                vkDestroyBuffer(device, vertexBuffer, nullptr);
                vkFreeMemory(device, vertexBufferMemory, nullptr);
            }
            CreateOrResizeBuffer(vertexBuffer, vertexBufferMemory, currentVertexBufferSize,
                vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }

        // Create or resize index buffer
        if (indexBuffer == VK_NULL_HANDLE || currentIndexBufferSize < indexBufferSize)
        {
            if (indexBuffer != VK_NULL_HANDLE)
            {
                std::cout << "Debug: Recreating index buffer" << std::endl;
                vkDestroyBuffer(device, indexBuffer, nullptr);
                vkFreeMemory(device, indexBufferMemory, nullptr);
            }
            CreateOrResizeBuffer(indexBuffer, indexBufferMemory, currentIndexBufferSize,
                indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        }

        // Map and upload data
        ImDrawVert* vtxDst = nullptr;
        ImDrawIdx* idxDst = nullptr;
        VkResult err;

        err = vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, (void**)&vtxDst);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to map vertex memory" << std::endl;
            return;
        }

        err = vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, (void**)&idxDst);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to map index memory" << std::endl;
            vkUnmapMemory(device, vertexBufferMemory);
            return;
        }

        // Copy the data
        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        // Unmap
        vkUnmapMemory(device, vertexBufferMemory);
        vkUnmapMemory(device, indexBufferMemory);

        std::cout << "Debug: Successfully uploaded draw data" << std::endl;
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


    void OffscreenPipeline::RecordCommandBuffer(ImDrawData* drawData)
    {
        auto const& device = Application::GetDevice();
        std::cout << "Debug: Recording command buffer with draw data:" << std::endl;
        std::cout << "  Display Size: " << drawData->DisplaySize.x << "x" << drawData->DisplaySize.y << std::endl;
        std::cout << "  Display Pos: " << drawData->DisplayPos.x << "," << drawData->DisplayPos.y << std::endl;
        std::cout << "  Framebuffer Scale: " << drawData->FramebufferScale.x << ","
            << drawData->FramebufferScale.y << std::endl;

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult err = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to begin command buffer" << std::endl;
            return;
        }

        // Initial transition to COLOR_ATTACHMENT_OPTIMAL
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = offscreenImage.GetVkImage();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { offscreenImage.GetWidth(), offscreenImage.GetHeight() };

        VkClearValue clearColor = { 0.2f, 0.3f, 0.3f, 1.0f };  // Teal color for debug
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        std::cout << "Debug: Setting clear color: " << clearColor.color.float32[0] << ","
            << clearColor.color.float32[1] << "," << clearColor.color.float32[2] << ","
            << clearColor.color.float32[3] << std::endl;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)offscreenImage.GetWidth();
        viewport.height = (float)offscreenImage.GetHeight();
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { offscreenImage.GetWidth(), offscreenImage.GetHeight() };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Set push constants
        float scale[2];
        float translate[2];
        scale[0] = 2.0f / offscreenImage.GetWidth();
        scale[1] = 2.0f / offscreenImage.GetHeight();
        translate[0] = -1.0f;
        translate[1] = -1.0f;

        std::cout << "Debug: Push constants:" << std::endl;
        std::cout << "  Scale: " << scale[0] << "," << scale[1] << std::endl;
        std::cout << "  Translate: " << translate[0] << "," << translate[1] << std::endl;

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(float) * 2, scale);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(float) * 2, sizeof(float) * 2, translate);

        // Bind vertex and index buffers
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // Debug buffer sizes
        VkMemoryRequirements vertexReqs, indexReqs;
        vkGetBufferMemoryRequirements(device, vertexBuffer, &vertexReqs);
        vkGetBufferMemoryRequirements(device, indexBuffer, &indexReqs);
        std::cout << "Debug: Buffer sizes:" << std::endl;
        std::cout << "  Vertex buffer size: " << vertexReqs.size << std::endl;
        std::cout << "  Index buffer size: " << indexReqs.size << std::endl;

        // Draw
        int vertexOffset = 0;
        int indexOffset = 0;
        std::cout << "Debug: Drawing " << drawData->CmdListsCount << " command lists" << std::endl;

        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmdList->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd_i];

                std::cout << "Debug: Command " << cmd_i << " clip rect: "
                    << pcmd->ClipRect.x << "," << pcmd->ClipRect.y << " -> "
                    << pcmd->ClipRect.z << "," << pcmd->ClipRect.w << std::endl;

                // Set scissor for this command
                VkRect2D cmdScissor;
                cmdScissor.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                cmdScissor.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                cmdScissor.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                cmdScissor.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(commandBuffer, 0, 1, &cmdScissor);

                // Bind descriptor set for texture
                VkDescriptorSet descSet = (VkDescriptorSet)pcmd->TextureId;
                std::cout << "Debug: Command texture ID: " << (uint64_t)pcmd->TextureId << std::endl;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 0, 1, &descSet, 0, nullptr);

                // Draw
                vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1,
                    indexOffset + pcmd->IdxOffset,
                    vertexOffset + pcmd->VtxOffset, 0);
            }
            indexOffset += cmdList->IdxBuffer.Size;
            vertexOffset += cmdList->VtxBuffer.Size;
        }

        vkCmdEndRenderPass(commandBuffer);

        // Transition to transfer source layout for reading back
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = offscreenImage.GetVkImage();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        err = vkEndCommandBuffer(commandBuffer);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to end command buffer" << std::endl;
            return;
        }

        std::cout << "Debug: Successfully recorded command buffer" << std::endl;
    }




    void OffscreenPipeline::Submit()
    {
        std::cout << "Debug: Starting Submit" << std::endl;

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkResult err = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to submit to queue" << std::endl;
            return;
        }

        err = vkQueueWaitIdle(queue);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to wait for queue idle" << std::endl;
            return;
        }

        std::cout << "Debug: Successfully submitted commands" << std::endl;
    }



}