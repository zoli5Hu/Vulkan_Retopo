#include "ResourceManager.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <tiny_obj_loader.h>

#include "VulkanUtils.h"

/**
 * @brief Konstruktor, inicializálja az erőforrás-kezelőt.
 * @param device A logikai eszköz (Logical Device), ami a memóriaműveleteket és foglalásokat végzi.
 * @param physicalDevice A fizikai videókártya, melynek tulajdonságai kellenek a megfelelő memóriatípus megtalálásához.
 */
ResourceManager::ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device(device), physicalDevice(physicalDevice) {}

ResourceManager::~ResourceManager() {}

/**
 * @brief Betölt egy 3D modellt (.obj) a fájlrendszerből, és kinyeri belőle a geometriát.
 * A memóriahatékonyság maximalizálása érdekében a rendszer a beolvasás során Hash-map
 * segítségével kiszűri a térben egybeeső, duplikált csúcspontokat (Vertexeket).
 * * @param filename A beolvasandó .obj fájl elérési útja.
 * @param vertices A kimeneti tömb, amibe az egyedi, ritkított 3D pontok kerülnek.
 * @param indices A kimeneti tömb, amibe a rajzolási topológia (háromszögek indexei) kerül.
 */
void ResourceManager::loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {

    // Nyers OBJ adatok tárolói
    tinyobj::attrib_t attrib;               // A térbeli pontok (x,y,z) globális listája
    std::vector<tinyobj::shape_t> shapes;   // A geometriai alakzatok (Mesh) és indexeik
    std::vector<tinyobj::material_t> materials; // Anyagjellemzők (pl. színek, textúrák)
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error("TinyObj hiba: " + warn + err);
    }

    // Hash-map az egyedi csúcspontok azonosítására és a VRAM pazarásának elkerülésére
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    // Végigmegyünk az összes beolvasott alakzaton és az azokat felépítő indexeken
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // 1. Pozíció (x, y, z) kiolvasása a nyers adatokból
            // A 3-as szorzó az 1D tömb iterációja miatt szükséges (gyorsabb, mint a maradékos osztás)
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // 2. Ideiglenes szín generálása a térbeli pozíció alapján (mivel jelenleg nincs textúra)
            // A std::max biztosítja, hogy a színértékek ne menjenek 0.0 (fekete) alá
            vertex.color = {
                std::max(0.0f, vertex.pos.x + 0.5f),
                std::max(0.0f, vertex.pos.y + 0.5f),
                std::max(0.0f, vertex.pos.z + 0.5f)
            };

            // 3. Duplikáció-ellenőrzés
            // Ha ezt a pontos Vertex (pos + color) konfigurációt még nem láttuk,
            // hozzáadjuk az egyedi listához, és feljegyezzük az indexét.
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            // 4. A csúcspont sorszámának mentése az Index Buffer számára
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    std::cout << "Modell betoltve: " << vertices.size() << " egyedi pont, " << indices.size() << " rajzolasi index." << std::endl;
}

/**
 * @brief Létrehoz egy Vertex Buffert a videókártya memóriájában (VRAM), és átmásolja bele a pontokat.
 * A folyamat 5 lépésből áll: Logikai foglalás -> Memóriaigény lekérdezése -> Fizikai allokáció -> Összekötés -> Adatmásolás.
 * * @param vertexBuffer Referencia a létrehozandó Vulkan puffer objektumra.
 * @param vertexBufferMemory Referencia a puffer mögötti dedikált GPU memóriára.
 * @param vertices A C++ által (CPU memóriában) tárolt térbeli pontok listája.
 */
void ResourceManager::createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory, const std::vector<Vertex> &vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // 1. Létrehozzuk a logikai "Tároló" objektumot
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // Típus: Vertex adat
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vertex Buffert!");
    }

    // 2. Lekérdezzük a kártyától, hogy ennek a puffernek mennyi és milyen fizikai memóriára van szüksége
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    // 3. Lefoglaljuk a fizikai memóriát (VRAM)
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Olyan memóriatípust keresünk, amit a CPU közvetlenül írhat (HOST_VISIBLE) és azonnal láthatóvá válik a GPU-nak (HOST_COHERENT)
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Vertex Buffernek!");
    }

    // 4. Összekapcsoljuk a logikai puffert a frissen lefoglalt fizikai memóriával
    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // 5. Adatok átmásolása (Mapping): Megnyitjuk a VRAM-ot a CPU számára, bemásoljuk a tömböt, majd bezárjuk.
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);
}

/**
 * @brief Létrehoz egy Index Buffert a GPU-n.
 * Az Index Buffer tárolja a rajzolási sorrendet (sorszámokat), amelyek a deduplikált Vertex Buffer pontjaira mutatnak.
 * Ennek használatával drasztikusan csökkenthető a VRAM sávszélesség-igénye geometriai renderelésnél.
 */
void ResourceManager::createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory, const std::vector<uint32_t> &indices) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // 1. Logikai puffer létrehozása
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // Típus: Index adat
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni az Index Buffert!");
    }

    // 2. Memóriaigény lekérdezése
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

    // 3. Fizikai memória lefoglalása
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni az Index Buffernek!");
    }

    // 4. Puffer és memória összekötése
    vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

    // 5. Adatok átmásolása (Mapping)
    void* data;
    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
}

/**
 * @brief Létrehozza a mélységi teszteléshez (Z-Buffer) szükséges képet és nézetet a memóriában.
 * A mélységi térkép biztosítja, hogy a térben közelebb lévő háromszögek helyesen takarják el a távolabbiakat.
 * * @param extent A textúra dimenziói (általában megegyezik a Swapchain / Ablak felbontásával).
 * @param depthFormat A Z-Buffer memóriatípusa (pl. 32-bites lebegőpontos, VK_FORMAT_D32_SFLOAT).
 * @param depthImage Referencia a létrehozandó Vulkan Image objektumra.
 * @param depthImageMemory Referencia a képhez lefoglalt, eszköz-specifikus (DEVICE_LOCAL) GPU memóriára.
 * @param depthImageView Referencia a képnézetre, amin keresztül a Pipeline hozzáfér az adatokhoz.
 */
void ResourceManager::createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage& depthImage, VkDeviceMemory& depthImageMemory, VkImageView& depthImageView) {

    // Kép (Image) létrehozása és memóriafoglalás a segédosztályon keresztül
    // A VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT jelzi, hogy ezt mélységi célpontként használjuk
    VulkanUtils::createImage(device, physicalDevice, extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

    // Képnézet (ImageView) létrehozása, ami meghatározza a textúra olvasási/írási módját
    depthImageView = VulkanUtils::createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}