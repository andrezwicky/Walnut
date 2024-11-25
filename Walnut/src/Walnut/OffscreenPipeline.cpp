#include "Walnut/Offscreenpipeline.h"
#include "Walnut/Utils.h"
#include <backends/imgui_impl_vulkan.h>

#include "imgui.h"
#include <array>

namespace Walnut
{
    
    OffscreenPipeline::OffscreenPipeline(VkQueue queue, OffscreenImage& image)
        : m_Queue(queue), m_OffscreenImage(image)
    {
        CreateRenderPass();
        CreateFramebuffer();
        CreatePipeline();
        AllocateCommandBuffer();
    }
    OffscreenPipeline::~OffscreenPipeline()
    {
        VkDevice device = Walnut::Application::GetDevice();

        vkDestroyFramebuffer(device, m_FrameBuffer, nullptr);
        vkDestroyPipeline(device, m_PipeLine, nullptr);
        vkDestroyPipelineLayout(device, m_PipeLineLayout, nullptr);
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        vkDestroyCommandPool(device, m_CommandPool, nullptr);

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
        colorAttachment.format = m_OffscreenImage.GetVkImageFormat();
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

        vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass);

        std::cout << "Debug: Render pass created successfully" << std::endl;
    }
    void OffscreenPipeline::CreateFramebuffer()
    {
        VkDevice device = Walnut::Application::GetDevice();

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_OffscreenImage.GetImageView();
        framebufferInfo.width = m_OffscreenImage.GetWidth();
        framebufferInfo.height = m_OffscreenImage.GetHeight();
        framebufferInfo.layers = 1;

        std::cout << "Debug: Creating framebuffer with dimensions: "
            << m_OffscreenImage.GetWidth() << "x"
            << m_OffscreenImage.GetHeight() << std::endl;

        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_FrameBuffer);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create offscreen framebuffer!");
    }
    void OffscreenPipeline::CreatePipeline()
    {
        auto const& device = Application::GetDevice();
        std::cout << "Debug: Creating pipeline with image dimensions: "
            << m_OffscreenImage.GetWidth() << "x" << m_OffscreenImage.GetHeight() << std::endl;

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

        err = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipeLineLayout);
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
        pipelineInfo.layout = m_PipeLineLayout;
        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PipeLine);
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

        vkCreateCommandPool(device, &poolInfo, nullptr, &m_CommandPool);

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(device, &allocInfo, &m_CommandBuffer);
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
        if (m_VertexBuffer == VK_NULL_HANDLE || m_CurrentVertexBufferSize < vertexBufferSize)
            CreateOrResizeBuffer(m_VertexBuffer, m_VertexBufferMemory, m_CurrentVertexBufferSize, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        // Create or resize index buffer
        if (m_IndexBuffer == VK_NULL_HANDLE || m_CurrentIndexBufferSize < indexBufferSize)
            CreateOrResizeBuffer(m_IndexBuffer, m_IndexBufferMemory, m_CurrentIndexBufferSize, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Map and upload data
        ImDrawVert* vtxDst = nullptr;
        ImDrawIdx* idxDst = nullptr;
        VkResult err;

        err = vkMapMemory(device, m_VertexBufferMemory, 0, vertexBufferSize, 0, (void**)&vtxDst);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to map vertex memory" << std::endl;
            return;
        }

        err = vkMapMemory(device, m_IndexBufferMemory, 0, indexBufferSize, 0, (void**)&idxDst);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to map index memory" << std::endl;
            vkUnmapMemory(device, m_VertexBufferMemory);
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
        vkUnmapMemory(device, m_VertexBufferMemory);
        vkUnmapMemory(device, m_IndexBufferMemory);

        std::cout << "Debug: Successfully uploaded draw data" << std::endl;
    }


    void OffscreenPipeline::CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize& currentSize, VkDeviceSize newSize, VkBufferUsageFlags usage)
    {
        auto const& device = Application::GetDevice();

        vkDeviceWaitIdle(device);

        // Destroy old buffer if it exists
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE; // Set to null to avoid dangling references
        }

        // Free old memory if it exists
        if (bufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, bufferMemory, nullptr);
            bufferMemory = VK_NULL_HANDLE; // Set to null to avoid double-free
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

        VkResult err = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
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
            barrier.image = m_OffscreenImage.GetVkImage();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                m_CommandBuffer,
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
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.framebuffer = m_FrameBuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { m_OffscreenImage.GetWidth(), m_OffscreenImage.GetHeight() };

        VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        std::cout << "Debug: Setting clear color: " << clearColor.color.float32[0] << ","
            << clearColor.color.float32[1] << "," << clearColor.color.float32[2] << ","
            << clearColor.color.float32[3] << std::endl;

        vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_OffscreenImage.GetWidth();
        viewport.height = (float)m_OffscreenImage.GetHeight();
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { m_OffscreenImage.GetWidth(), m_OffscreenImage.GetHeight() };
        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

        // Bind pipeline
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipeLine);

        // Set push constants
        float scale[2];
        float translate[2];
        scale[0] = 2.0f / m_OffscreenImage.GetWidth();
        scale[1] = 2.0f / m_OffscreenImage.GetHeight();
        translate[0] = -1.0f;
        translate[1] = -1.0f;

        std::cout << "Debug: Push constants:" << std::endl;
        std::cout << "  Scale: " << scale[0] << "," << scale[1] << std::endl;
        std::cout << "  Translate: " << translate[0] << "," << translate[1] << std::endl;

        vkCmdPushConstants(m_CommandBuffer, m_PipeLineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(float) * 2, scale);
        vkCmdPushConstants(m_CommandBuffer, m_PipeLineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(float) * 2, sizeof(float) * 2, translate);

        // Bind vertex and index buffers
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &m_VertexBuffer, offsets);
        vkCmdBindIndexBuffer(m_CommandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // Debug buffer sizes
        VkMemoryRequirements vertexReqs, indexReqs;
        vkGetBufferMemoryRequirements(device, m_VertexBuffer, &vertexReqs);
        vkGetBufferMemoryRequirements(device, m_IndexBuffer, &indexReqs);
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
                vkCmdSetScissor(m_CommandBuffer, 0, 1, &cmdScissor);

                // Bind descriptor set for texture
                VkDescriptorSet descSet = (VkDescriptorSet)pcmd->TextureId;
                std::cout << "Debug: Command texture ID: " << (uint64_t)pcmd->TextureId << std::endl;
                vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PipeLineLayout, 0, 1, &descSet, 0, nullptr);

                // Draw
                vkCmdDrawIndexed(m_CommandBuffer, pcmd->ElemCount, 1,
                    indexOffset + pcmd->IdxOffset,
                    vertexOffset + pcmd->VtxOffset, 0);
            }
            indexOffset += cmdList->IdxBuffer.Size;
            vertexOffset += cmdList->VtxBuffer.Size;
        }

        vkCmdEndRenderPass(m_CommandBuffer);

        // Transition to transfer source layout for reading back
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_OffscreenImage.GetVkImage();
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(
                m_CommandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        }

        err = vkEndCommandBuffer(m_CommandBuffer);
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
        submitInfo.pCommandBuffers = &m_CommandBuffer;

        VkResult err = vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to submit to queue" << std::endl;
            return;
        }

        err = vkQueueWaitIdle(m_Queue);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to wait for queue idle" << std::endl;
            return;
        }

        std::cout << "Debug: Successfully submitted commands" << std::endl;
    }



}