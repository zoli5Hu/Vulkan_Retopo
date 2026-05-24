#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "Vertex.h"

class ResourceManager {
public:
    ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);

    ~ResourceManager();

    // Modell adatok
    void loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

    // Buffer kezelés
    void createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory,
                            const std::vector<Vertex> &vertices);

    void createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory,
                           const std::vector<uint32_t> &indices);

    // Mélység kezelés (Z-Buffer)
    void createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage &depthImage,
                              VkDeviceMemory &depthImageMemory, VkImageView &depthImageView);

    // ---> EZ A KÉT SOR MARADT LE KORÁBBAN: <---
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                 VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    // Segédfüggvények
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
};
