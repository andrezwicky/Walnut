#pragma once
#include <iostream>
#include <string>
#include <memory>

#include "vulkan/vulkan.h"
#include "Walnut/Utils.h"


namespace Walnut
{
	class OffscreenImage
	{
	public:
		OffscreenImage(uint32_t width, uint32_t height, ImageFormat format);
		~OffscreenImage();

		VkImage GetVkImage() { return m_Image; }
		//VkImageView GetImageView() const { return m_ImageView; }
		VkImageView& GetImageView() { return m_ImageView; }
		VkFormat GetVkImageFormat() const { return Utils::WalnutFormatToVulkanFormat(m_Format); }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		void ReadBack(void* data, size_t dataSize);

		

		

	private:
		void AllocateMemory(uint64_t size, bool makesource = false);
		// Allocate memory for the Vulkan image.
		// If makesource is true, the image is configured for CPU-GPU data transfers (e.g., uploading or downloading image data).
		// Otherwise, the image is optimized for rendering or sampling.

		void Release();

	private:
		ImageFormat m_Format = ImageFormat::None;
		uint32_t m_Width = 0, m_Height = 0;

		VkImage m_Image = nullptr;
		VkImageView m_ImageView = nullptr;
		VkDeviceMemory m_Memory = nullptr;

		

		VkBuffer m_StagingBuffer = nullptr;
		VkDeviceMemory m_StagingBufferMemory = nullptr;

		VkBuffer m_unStagingBuffer = nullptr;
		VkDeviceMemory m_unStagingBufferMemory = nullptr;

		size_t m_AlignedSize = 0;

		VkDescriptorSet m_DescriptorSet = nullptr;

		void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

		VkCommandBuffer BeginSingleTimeCommands(VkCommandPool commandPool);
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, VkQueue queue);

	};
}


