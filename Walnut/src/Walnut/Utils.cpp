#include "Walnut/Utils.h"

uint32_t Walnut::Utils::GetVulkanMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
	VkPhysicalDeviceMemoryProperties prop;
	vkGetPhysicalDeviceMemoryProperties(Walnut::Application::GetPhysicalDevice(), &prop);
	for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
	{
		if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
			return i;
	}

	return 0xffffffff;
}

uint32_t Walnut::Utils::BytesPerPixel(ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::GBR3P:	return 3;
	case ImageFormat::RGBA:		return 4;
	case ImageFormat::RGBA32F:	return 16;
	}
	return 0;
}

VkFormat Walnut::Utils::WalnutFormatToVulkanFormat(ImageFormat format)
{
	switch (format)
	{
	case ImageFormat::GBR3P:	return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
	case ImageFormat::RGBA:		return VK_FORMAT_R8G8B8A8_UNORM;
	case ImageFormat::RGBA32F:	return VK_FORMAT_R32G32B32A32_SFLOAT;
	}
	return (VkFormat)0;
}

