#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "Vertex.h"

/**
 * @struct ModelData
 * @brief Egyetlen 3D modell (pl. High Poly vagy Low Poly háló) összes adatát összefogó struktúra.
 * Tartalmazza a CPU memóriában lévő nyers geometriát, valamint a hozzájuk tartozó lefoglalt
 * GPU memóriaterületeket (Vulkan Buffereket) és állapotjelzőket. Ezt a struktúrát küldjük be a renderelőnek.
 */
struct ModelData {
    std::vector<Vertex> vertices;                  /**< A modell egyedi térbeli pontjainak listája a RAM-ban. */
    std::vector<uint32_t> indices;                 /**< A rajzolási sorrendet (háromszögeket) meghatározó indexek. */
    VkBuffer vertexBuffer = VK_NULL_HANDLE;        /**< A GPU-n lefoglalt logikai tároló a térbeli pontoknak. */
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE; /**< A Vertex Buffer mögötti dedikált fizikai VRAM terület. */
    VkBuffer indexBuffer = VK_NULL_HANDLE;         /**< A GPU-n lefoglalt logikai tároló az indexeknek. */
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;  /**< Az Index Buffer mögötti dedikált fizikai VRAM terület. */
    bool isLoaded = false;                         /**< Igaz, ha a modell hibamentesen betöltődött a videókártyára. */
};

/**
 * @class ResourceManager
 * @brief A CPU és GPU közötti adatmozgatásért, memóriafoglalásért és modellbetöltésért felelős osztály.
 * * Ez az osztály végzi el az OBJ fájlok beolvasását, a duplikált csúcspontok szűrését,
 * valamint a Vulkan specifikus memóriaterületek (Vertex, Index, Depth Bufferek) fizikai allokációját és a másolást.
 */
class ResourceManager {
public:
    /**
     * @brief Konstruktor, amely átveszi a hardveres eszközök referenciáit a memóriafoglaláshoz.
     * @param device A logikai eszköz, ami a Vulkan memóriaparancsokat fogadja.
     * @param physicalDevice A fizikai videókártya, melynek képességeit és memóriatípusait vizsgáljuk.
     */
    ResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);

    ~ResourceManager();

    /**
     * @brief Nyers 3D modell adatokat (.obj) tölt be a fájlrendszerből a RAM-ba, duplikáció-szűréssel.
     * @param filename Az OBJ fájl relatív vagy abszolút elérési útja.
     * @param vertices Kimeneti paraméter, amibe a ritkított, egyedi csúcspontok kerülnek.
     * @param indices Kimeneti paraméter, amibe a topológiát leíró indexsorrend kerül.
     */
    void loadModel(const std::string &filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

    /**
     * @brief Létrehoz egy Vertex Buffert a GPU-n, és átmásolja bele a térbeli pontokat a RAM-ból.
     * @param vertexBuffer Hivatkozás a létrehozandó tárolóra (Handle).
     * @param vertexBufferMemory Hivatkozás a dedikált fizikai VRAM memóriára.
     * @param vertices A másolandó (forrás) adatokat tartalmazó C++ tömb.
     */
    void createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory,
                            const std::vector<Vertex> &vertices);

    /**
     * @brief Létrehoz egy Index Buffert a GPU-n, és átmásolja bele a rajzolási sorszámokat a RAM-ból.
     * @param indexBuffer Hivatkozás a létrehozandó tárolóra (Handle).
     * @param indexBufferMemory Hivatkozás a dedikált fizikai VRAM memóriára.
     * @param indices A másolandó (forrás) adatokat tartalmazó C++ tömb.
     */
    void createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory,
                           const std::vector<uint32_t> &indices);

    /**
     * @brief Képet és memóriát foglal a videókártyán a mélységi teszteléshez (Z-Buffer).
     * @param extent A textúra felbontása (szélesség, magasság).
     * @param depthFormat A mélységi adatok formátuma (pl. VK_FORMAT_D32_SFLOAT).
     * @param depthImage Referencia a létrehozandó textúrára.
     * @param depthImageMemory Referencia a képhez lefoglalt, csak GPU által látható (DEVICE_LOCAL) memóriára.
     * @param depthImageView Referencia a képnézetre, amin keresztül a csővezeték olvassa a textúrát.
     */
    void createDepthResources(VkExtent2D extent, VkFormat depthFormat, VkImage &depthImage,
                              VkDeviceMemory &depthImageMemory, VkImageView &depthImageView);

private:
    /** @brief A logikai eszköz, ami a memóriafoglalási (vkAllocateMemory) utasításokat fogadja. */
    VkDevice device;

    /** @brief A fizikai eszköz, amiből a hardveres memóriatulajdonságokat (pl. HOST_VISIBLE) lekérdezzük. */
    VkPhysicalDevice physicalDevice;
};