#include "ResourceManager.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <tiny_obj_loader.h>

#include "VulkanUtils.h"

//vulkan erroforrások betöltése
ResourceManager::ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device(device), physicalDevice(physicalDevice) {}

ResourceManager::~ResourceManager() {}

// - loadModel
void ResourceManager::loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {

    //ponthalmazd és obj adatok
    tinyobj::attrib_t attrib;
    //megmondja mettől meddig van 1 objektum adatai
    std::vector<tinyobj::shape_t> shapes;
    //tulajdonság liusta pl szín
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "modells/kocka.obj")) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    //elkezdjük a duplikált vertexeket egy olyan mapbe rakni ahol már nem a duplikált vertexek vannak és látjuk melyik mi után jön
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // 1. Pozíció kiolvasása a nyers OBJ adatokból
            //azért szorzunk mert a maradéskos osztás lassú
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // 2. Szín beállítása (mivel az obj-ben nincs szín, legyen sima fehér+pos)
            //a maxal le clampeljük
            vertex.color = {
                std::max(0.0f, vertex.pos.x + 0.5f),
                std::max(0.0f, vertex.pos.y + 0.5f),
                std::max(0.0f, vertex.pos.z + 0.5f)
            };
            // 3.  Ha ezt a pontot még nem láttuk, mentsük el!
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



// - createVertexBuffer itt adjuk meg hogy mekkora területet akarunk lefoglalni
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
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
// Indexbuffer létrehozása: ez tárolja a rajzolási sorrendet (sorszámokat), amik a ritkított pontokra mutatnak
void ResourceManager::createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory, const std::vector<uint32_t> &indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // <-- INDEX BUFFER!
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni az Index Buffert!");
    }
    //tényleges memória lefoglalása a gpun  buffer alapján
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni az Index Buffernek!");
    }
    //hozzákötjük a buffer adatokat memóriához
    vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);
    //tényyleges másolás
    void* data;
    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
}


// - createDepthResources (és a segédjei: createImage, createImageView, findMemoryType, findDepthFormat)
// --- MÉLYSÉG-TÁROLÓ (Z-BUFFER) SEGÉDFÜGGVÉNYEI ---


// 3. Létrehozza a mélység-tároló képet, memóriát és képnézetet
void ResourceManager::createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage& depthImage, VkDeviceMemory& depthImageMemory, VkImageView& depthImageView) {

    // Itt a paraméterből kapott extent.width és extent.height-et használjuk!
    VulkanUtils::createImage(device, physicalDevice, extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

    depthImageView = VulkanUtils::createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}
