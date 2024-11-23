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
}