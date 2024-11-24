#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>
#include "Walnut/Application.h"
#include "Walnut/Types.h"

namespace Walnut
{
	class Utils
	{
	public:
		static uint32_t GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
		static uint32_t BytesPerPixel(ImageFormat format);
		static VkFormat WalnutFormatToVulkanFormat(ImageFormat format);
		static uint32_t GetGraphicsQueueFamilyIndex(VkPhysicalDevice physicalDevice);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	};
}
