#include "VulkanUtils.h"

/**
 * @brief Létrehoz egy képnézetet (Image View) egy memóriában lévő nyers textúrához/képhez.
 * A Vulkan sosem olvas vagy ír közvetlenül egy VkImage objektumba. Mindig szüksége van
 * egy "lencsére" (ImageView), ami megmondja, hogy a kép melyik részét, milyen formátumban
 * (pl. csak a színcsatornát, vagy a mélységcsatornát) akarjuk használni.
 * * @param device A Vulkan logikai eszköz.
 * @param image A nyers kép, amire a lencsét illesztjük.
 * @param format A kép pixelformátuma.
 * @param aspectFlags Kép aspektusa (VK_IMAGE_ASPECT_COLOR_BIT vagy VK_IMAGE_ASPECT_DEPTH_BIT).
 * @return VkImageView A létrehozott képnézet.
 */
VkImageView VulkanUtils::createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 2D-s textúraként kezeljük
    viewInfo.format = format;

    // Melyik részét használjuk a képnek? Nincsenek MipMap-ek és rétegek (Layers).
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

/**
 * @brief Megkeresi a hardvernek és a mi igényeinknek egyaránt megfelelő memóriatípust a GPU-n.
 * A videókártya memóriája (VRAM) különböző "hegyekre" (Heaps) van osztva. Van, ami gyors a
 * GPU-nak, de a CPU nem éri el, és van, amit a CPU is lát (HOST_VISIBLE). Ez a függvény
 * bitmaszkok alapján kiválasztja a megfelelőt.
 * * @param physicalDevice A fizikai videókártya, amelynek memóriatulajdonságait lekérdezzük.
 * @param typeFilter Bitmaszk a számunkra elfogadható memóriatípusokról.
 * @param properties A memória elvárt tulajdonságai (pl. CPU által írható legyen).
 * @return uint32_t A kiválasztott memóriatípus sorszáma (indexe).
 */
uint32_t VulkanUtils::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Ha a típus megfelelő ÉS a tulajdonságok bitmaszkja is 100%-ban megegyezik a kért tulajdonságokkal
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Hiba: Nem talalhato megfelelo tipusu memoria a GPU-n!");
}

/**
 * @brief Létrehoz egy logikai Vulkan képet, és lefoglalja számára a fizikai memóriát.
 * A Vulkanban ez egy 3 lépéses folyamat: Logikai kép -> Memóriaigény felmérése -> VRAM allokáció és összekötés.
 */
void VulkanUtils::createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) {

    // 1. Logikai kép (Image) objektum létrehozása
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling; // OPTIMAL (GPU-barát) vagy LINEAR (CPU-barát) elrendezés
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a kepet!");
    }

    // 2. Megkérdezzük, mennyi byte fizikai VRAM kell ennek a képnek az adott kártyán
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    // 3. Lefoglaljuk az eszköztől kért pontos memóriamennyiséget a megfelelő memóriablokkból
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a kepnek!");
    }

    // 4. A logikai kép objektum összekötése a lefoglalt fizikai memóriával (VRAM)
    vkBindImageMemory(device, image, imageMemory, 0);
}

/**
 * @brief Dinamikusan megkeresi a hardver által támogatott legjobb pixelformátumot.
 * Mivel Windows, Mac és Linux alatt a videókártyák eltérő pixel elrendezéseket támogatnak,
 * ez a függvény egy preferencia-listából (candidates) kiválasztja az első olyat, ami működik.
 */
VkFormat VulkanUtils::findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        // Ellenőrizzük, hogy a kiválasztott csempézési (tiling) mód támogatja-e a kért funkciókat
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Hiba: Nem talalhato tamogatott formatum a kartyan!");
}

/**
 * @brief Megkeresi a Z-Buffer (Mélységi teszt) számára legideálisabb formátumot a videókártyán.
 * Preferálja a 32-bites lebegőpontos (D32_SFLOAT) formátumot, de visszaesik 24-bitesre, ha
 * az adott hardver nem támogatja (pl. régebbi integrált kártyák esetén).
 * * @param physicalDevice A fizikai videókártya.
 * @return VkFormat A hardver által támogatott legjobb mélységi formátum.
 */
VkFormat VulkanUtils::findDepthFormat(VkPhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT // Fontos, hogy mélység-csatolmányként lehessen használni
    );
}