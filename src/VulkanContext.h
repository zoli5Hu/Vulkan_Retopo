#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

/**
 * @struct QueueFamilyIndices
 * @brief A videókártyán található parancssorok (Queue Families) indexeit tárolja.
 * A Vulkanban a GPU különböző részlegekre van osztva. Külön részleg felelhet a
 * matematikai rajzolásért (Graphics) és a képernyőre küldésért (Present).
 */
struct QueueFamilyIndices {
    /** @brief A rajzolási (3D renderelés, másolás) parancsokat fogadó sor indexe. */
    std::optional<uint32_t> graphicsFamily;

    /** @brief A monitorral való kommunikációt (kép megjelenítése) végző sor indexe. */
    std::optional<uint32_t> presentFamily;

    /**
     * @brief Ellenőrzi, hogy a GPU rendelkezik-e minden szükséges részleggel a program futtatásához.
     * @return Igaz, ha mind a grafikus, mind a megjelenítési sor megtalálható.
     */
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @struct SwapChainSupportDetails
 * @brief A videókártya képmegjelenítési (Swapchain) képességeit tároló struktúra.
 */
struct SwapChainSupportDetails {
    /** @brief A képernyő/ablak fizikai határai (pl. minimális és maximális felbontás, képek száma). */
    VkSurfaceCapabilitiesKHR capabilities;

    /** @brief A GPU által támogatott pixelformátumok és színterek (pl. 8-bites SRGB, HDR). */
    std::vector<VkSurfaceFormatKHR> formats;

    /** @brief A támogatott képfrissítési stratégiák (VSync módok, pl. MAILBOX, FIFO). */
    std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @class VulkanContext
 * @brief A Vulkan grafikus API alaprendszerét inicializáló és tároló magosztály.
 * * Ez az osztály építi fel a kapcsolatot a szoftver és a fizikai videókártya között.
 * Létrehozza az ablakfelületet, kiválasztja a GPU-t, beállítja a parancssorokat, és
 * inicializálja a képernyőre rajzoláshoz szükséges Swapchain-t.
 */
class VulkanContext {
public:
    /**
     * @brief Konstruktor, amely a megadott ablakra felépíti a teljes Vulkan környezetet.
     * @param window Mutató a GLFW által létrehozott operációs rendszeri ablakra.
     */
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    // ========================================================================
    // PUBLIKUS VULKAN ERŐFORRÁSOK (A többi osztály ezekre hivatkozik)
    // ========================================================================

    /** @brief A Vulkan API példánya, a fő híd az alkalmazás és a Vulkan driver között. */
    VkInstance instance;

    /** @brief A validációs rétegek (hibakereső) üzenetküldő rendszere. */
    VkDebugUtilsMessengerEXT debugMessenger;

    /** @brief A platformfüggetlen felület (vászon), amely összeköti a Vulkant a GLFW ablakkal. */
    VkSurfaceKHR surface;

    /** @brief Maga a kiválasztott fizikai videókártya (Hardware). */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    /** @brief A logikai eszköz (Software Controller), ezen keresztül küldjük a parancsokat a GPU-nak. */
    VkDevice device;

    /** @brief A csatorna (Queue), ahova a 3D rajzolási parancslistákat küldjük be. */
    VkQueue graphicsQueue;

    /** @brief A csatorna (Queue), ahova a "Tedd ki a képernyőre!" (Present) parancsok mennek. */
    VkQueue presentQueue;

    /** @brief A Swapchain (Képcserélő lánc) az egyenletes, képtörésmentes (VSync) megjelenítésért. */
    VkSwapchainKHR swapChain;

    /** @brief A Swapchain által lefoglalt és felügyelt nyers képek (Vásznak) listája. */
    std::vector<VkImage> swapChainImages;

    /** @brief A Swapchain képeinek kiválasztott pixelformátuma (pl. VK_FORMAT_B8G8R8A8_SRGB). */
    VkFormat swapChainImageFormat;

    /** @brief A Swapchain képeinek pontos felbontása (általában megegyezik az ablakmérettel). */
    VkExtent2D swapChainExtent;

    /** @brief A képekre helyezett lencsék (Nézetek), amelyeken keresztül a shader olvasni/írni tud. */
    std::vector<VkImageView> swapChainImageViews;

private:
    /** @brief Hivatkozás a GLFW ablakra. */
    GLFWwindow* window;

    // --- Inicializáló függvények (A konstruktor futtatja őket sorrendben) ---
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();

    // --- Hardver ellenőrző és logikai segédfüggvények ---
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};