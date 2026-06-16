#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

/**
 * @class VulkanUtils
 * @brief Statikus segédfüggvényeket tartalmazó eszköztár (Utility class) a Vulkan API-hoz.
 * * Ez az osztály összefogja a gyakran ismétlődő, "boilerplate" Vulkan műveleteket,
 * mint például a textúrák/képek memóriafoglalása és a hardverspecifikus formátumok lekérdezése.
 * Mivel a függvények statikusak, az osztályt nem kell példányosítani.
 */
class VulkanUtils {
public:
    /**
     * @brief Létrehoz egy "lencsét" (Image View), amin keresztül a GPU értelmezni tudja a nyers képadatokat.
     * @param device A Vulkan logikai eszköz.
     * @param image A nyers kép, amelyhez a nézetet készítjük.
     * @param format A kép pixelformátuma (pl. szín vagy mélység).
     * @param aspectFlags Meghatározza a kép célját (pl. VK_IMAGE_ASPECT_COLOR_BIT).
     * @return VkImageView A létrehozott képnézet.
     */
    static VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    /**
     * @brief Megkeresi a GPU fizikai memóriájában (VRAM) a kért tulajdonságoknak megfelelő blokkot.
     * @param physicalDevice A fizikai videókártya.
     * @param typeFilter Bitmaszk, amely a számunkra elfogadható memóriatípusokat jelöli.
     * @param properties A memóriával szemben támasztott követelmények (pl. CPU által írható).
     * @return uint32_t A megfelelő memóriatípus sorszáma (indexe).
     */
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    /**
     * @brief Létrehoz egy képet a memóriában, és elvégzi a fizikai VRAM allokációt is.
     * @param device A logikai eszköz.
     * @param physicalDevice A fizikai kártya (a memóriatípus kereséséhez).
     * @param width A kép szélessége pixelben.
     * @param height A kép magassága pixelben.
     * @param format A kép formátuma.
     * @param tiling GPU-barát (Optimal) vagy CPU-barát (Linear) bájtelrendezés.
     * @param usage Mire fogjuk használni a képet (pl. mélység-csatolmány).
     * @param properties Memória tulajdonságok (pl. lokális a GPU-n).
     * @param image [Kimenet] A létrejött logikai kép handle-je.
     * @param imageMemory [Kimenet] A képhez rendelt fizikai memória handle-je.
     */
    static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);

    /**
     * @brief Végigiterál egy formátum-listán, és visszaadja az elsőt, amit a hardver támogat.
     * @param physicalDevice A fizikai videókártya.
     * @param candidates A preferált formátumok listája (a legjobbtól a legkevésbé jóig).
     * @param tiling A használni kívánt elrendezési mód.
     * @param features A formátumtól elvárt képességek (Feature flags).
     * @return VkFormat A hardver által támogatott legjobb formátum.
     */
    static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    /**
     * @brief Megkeresi a videókártyán elérhető legideálisabb Z-Buffer (Mélységi teszt) formátumot.
     * @param physicalDevice A fizikai videókártya.
     * @return VkFormat A támogatott mélységi formátum (pl. VK_FORMAT_D32_SFLOAT).
     */
    static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
};