#include "ResourceManager.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <tiny_obj_loader.h>

#include "RetopoApp.h"

ResourceManager::ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device(device), physicalDevice(physicalDevice) {}

ResourceManager::~ResourceManager() {}

// IDE MÁSOLD ÁT a RetopoApp-ból:
// - loadModel
void ResourceManager::loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "modells/kocka.obj")) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // 1. Pozíció kiolvasása a nyers OBJ adatokból
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // 2. Szín beállítása (mivel az obj-ben nincs szín, legyen sima fehér)
            vertex.color = {
                std::max(0.0f, vertex.pos.x + 0.5f),
                std::max(0.0f, vertex.pos.y + 0.5f),
                std::max(0.0f, vertex.pos.z + 0.5f)
            };
            // 3. A ZSENIÁLIS TRÜKK: Ha ezt a pontot még nem láttuk, mentsük el!
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            // 4. Mentsük el a sorszámot (indexet) a rajzolási listába
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    std::cout << "Modell betoltve: " << vertices.size() << " egyedi pont, " << indices.size() << " index." << std::endl;

}



// - createVertexBuffer
void ResourceManager::createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory, const std::vector<Vertex> &vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // 1. Létrehozzuk magát a "Tároló" objektumot
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // Ez egy Vertex Buffer lesz
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vertex Buffert!");
    }

    // 2. Megkérdezzük a Vulkant, mennyi és milyen memóriára van szüksége ennek a tárolónak
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    // 3. Lefoglaljuk a fizikai memóriát (VRAM)
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Olyan memóriát kérünk, amit a C++ (CPU) is lát, és azonnal szinkronizálódik
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Vertex Buffernek!");
    }

    // 4. Összekötjük a tárolót a lefoglalt memóriával
    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // 5. AZ ADATOK ÁTMÁSOLÁSA (C++ -> GPU)
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data); // Kinyitjuk a memóriát
    memcpy(data, vertices.data(), (size_t) bufferSize);               // Bemásoljuk a C++ tömböt
    vkUnmapMemory(device, vertexBufferMemory);
}

// - createIndexBuffer
void ResourceManager::createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory, const std::vector<uint32_t> &indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // <-- FIGYELEM: INDEX BUFFER!
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni az Index Buffert!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni az Index Buffernek!");
    }

    vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

    void* data;
    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
}

//segédfügvények
uint32_t ResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Hiba: Nem talalhato megfelelo tipusu memoria a GPU-n!");

}

void ResourceManager::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a kepet!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a kepnek!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView ResourceManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni az Image View-t!");
    }
    return imageView;
}

// - createDepthResources (és a segédjei: createImage, createImageView, findMemoryType, findDepthFormat)
// --- MÉLYSÉG-TÁROLÓ (Z-BUFFER) SEGÉDFÜGGVÉNYEI ---

// 1. Átköltözött a ResourceManagerbe!
VkFormat ResourceManager::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Hiba: Nem talalhato tamogatott formatum a kartyan!");
}

// 2. Átköltözött a ResourceManagerbe! (Nem kell a RetopoApp::)
VkFormat ResourceManager::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

// 3. A fő függvény javítva az új paraméterekkel
void ResourceManager::createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage& depthImage, VkDeviceMemory& depthImageMemory, VkImageView& depthImageView) {

    // Itt a paraméterből kapott extent.width és extent.height-et használjuk!
    createImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}
