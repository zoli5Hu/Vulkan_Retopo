#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "Vertex.h"


// ÚJ: Ez a struktúra tárolja egyetlen modell minden adatát
struct ModelData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    bool isLoaded = false;
};

class ResourceManager {
public:


    ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);

    ~ResourceManager();

    // Modell adatok
    void loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

    // Buffer kezelés vertex adatok
    void createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory,
                            const std::vector<Vertex> &vertices);
    //a vertex indexek buffere
    void createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory,
                           const std::vector<uint32_t> &indices);

    // Mélység kezelés (Z-Buffer)
    void createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage &depthImage,
                              VkDeviceMemory &depthImageMemory, VkImageView &depthImageView);


private:
    //a változó amin keresztül kommunikálunk a gpuval
    VkDevice device;
    //Maga a kiválasztott fizikai videókártya (ebből kérdezzük le a hardver képességeit)
    VkPhysicalDevice physicalDevice;
};
