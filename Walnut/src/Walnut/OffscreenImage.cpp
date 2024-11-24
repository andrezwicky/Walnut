//#include <backends/imgui_impl_vulkan.h>

#include "Walnut/Application.h"
#include "OffscreenImage.h"


namespace Walnut
{
	OffscreenImage::OffscreenImage(uint32_t width, uint32_t height, ImageFormat format)
		: m_Width(width), m_Height(height), m_Format(format)
	{
		AllocateMemory(m_Width * m_Height * Utils::BytesPerPixel(m_Format));
	}
	OffscreenImage::~OffscreenImage()
	{
		Release();
	}

    void OffscreenImage::AllocateMemory(uint64_t size, bool makesource) {
        VkDevice device = Application::GetDevice();
        VkResult err;

        VkFormat vulkanFormat = Utils::WalnutFormatToVulkanFormat(m_Format);

        // Create the Vulkan image
        {
            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = vulkanFormat;
            imageInfo.extent = { m_Width, m_Height, 1 };
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = makesource ? (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            err = vkCreateImage(device, &imageInfo, nullptr, &m_Image);
            check_vk_result(err);

            // Allocate memory for the image
            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(device, m_Image, &memRequirements);

            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Utils::GetVulkanMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements.memoryTypeBits);

            err = vkAllocateMemory(device, &allocInfo, nullptr, &m_Memory);
            check_vk_result(err);

            err = vkBindImageMemory(device, m_Image, m_Memory, 0);
            check_vk_result(err);
        }

        if (makesource) return;

        // Create the image view
        {
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_Image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = vulkanFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            err = vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView);
            check_vk_result(err);
        }
    }

	void OffscreenImage::Release()
	{
		Application::SubmitResourceFree([imageView = m_ImageView, image = m_Image,
			memory = m_Memory, stagingBuffer = m_StagingBuffer, stagingBufferMemory = m_StagingBufferMemory, unstagingBuffer = m_unStagingBuffer, unstagingBufferMemory = m_unStagingBufferMemory]()
			{
				VkDevice device = Application::GetDevice();

				vkDestroyImageView(device, imageView, nullptr);
				vkDestroyImage(device, image, nullptr);
				vkFreeMemory(device, memory, nullptr);
				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingBufferMemory, nullptr);
				vkDestroyBuffer(device, unstagingBuffer, nullptr);
				vkFreeMemory(device, unstagingBufferMemory, nullptr);
			});

		m_ImageView = nullptr;
		m_Image = nullptr;
		m_Memory = nullptr;
		m_StagingBuffer = nullptr;
		m_StagingBufferMemory = nullptr;
		m_unStagingBuffer = nullptr;
		m_unStagingBufferMemory = nullptr;
	}

    void OffscreenImage::ReadBack(void* data, size_t dataSize)
    {
        std::cout << "Debug: Starting image readback, size: " << dataSize << " bytes" << std::endl;

        VkDevice device = Application::GetDevice();

        // Create a staging buffer for readback
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = dataSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult err = vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to create staging buffer" << std::endl;
            return;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        err = vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to allocate staging buffer memory" << std::endl;
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            return;
        }

        vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

        // Copy image to buffer
        auto& commandPool = *Application::Get().GetCommandPool();
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands(&commandPool);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { m_Width, m_Height, 1 };

        std::cout << "Debug: Copying image to buffer, dimensions: " << m_Width << "x" << m_Height << std::endl;

        vkCmdCopyImageToBuffer(
            commandBuffer,
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingBuffer,
            1,
            &region
        );

        auto& queue = *Application::Get().GetQueue();
        EndSingleTimeCommands(commandBuffer, &commandPool, &queue);

        // Read data from staging buffer
        void* mappedData;
        err = vkMapMemory(device, stagingBufferMemory, 0, dataSize, 0, &mappedData);
        if (err != VK_SUCCESS) {
            std::cout << "Error: Failed to map memory" << std::endl;
            return;
        }

        memcpy(data, mappedData, dataSize);
        vkUnmapMemory(device, stagingBufferMemory);

        // Cleanup
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        std::cout << "Debug: Successfully completed image readback" << std::endl;
    }

    void OffscreenImage::TransitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // Transition from undefined to transfer destination
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // Transition from transfer destination to shader read
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            // Transition from color attachment to transfer source
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            // Transition back from transfer source to color attachment
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }



    VkCommandBuffer OffscreenImage::BeginSingleTimeCommands(VkCommandPool commandPool)
    {
        VkDevice device = Walnut::Application::GetDevice();

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer!");
        }

        return commandBuffer;
    }
    void OffscreenImage::EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue)
    {
        VkDevice device = Walnut::Application::GetDevice();

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer!");
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer!");
        }

        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

}