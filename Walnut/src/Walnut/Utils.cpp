#include "Walnut/Utils.h"
#include <stdexcept>

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
uint32_t Walnut::Utils::GetGraphicsQueueFamilyIndex(VkPhysicalDevice physicalDevice) {
	uint32_t queueFamilyCount = 0;

	// Query the number of queue families
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	if (queueFamilyCount == 0) {
		throw std::runtime_error("Failed to find any queue families on the physical device!");
	}

	// Retrieve the properties of each queue family
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	// Find a queue family that supports graphics
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			return i; // Return the index of the graphics queue family
		}
	}

	// If no graphics queue is found, throw an error
	throw std::runtime_error("Failed to find a graphics queue family!");
}

uint32_t Walnut::Utils::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDevice physicalDevice = Walnut::Application::GetPhysicalDevice(); // Make sure you have access to the physical device

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}


