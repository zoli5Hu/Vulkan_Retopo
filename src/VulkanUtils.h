#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class VulkanUtils {
public:
    // Képnézet lencse készítése (Ide bekerült a VkDevice paraméter!)
    static VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    // Memória típus keresése (Ide bekerült a VkPhysicalDevice paraméter!)
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Üres kép létrehozása a memóriában
    static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);

    // Támogatott formátumok lekérdezése
    static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Legjobb mélység (Z-Buffer) formátum keresése
    static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
};