#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

// Ezek a struktúrák átköltöztek ide a RetopoApp.h-ból!
struct QueueFamilyIndices {
    //a kép kirajzolásahoz szükséges parancsok (pl. rajzolás, buffer másolás)
    std::optional<uint32_t> graphicsFamily;
    //kép megjelenítése monitoron
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
//azok az adatok amiket támogat a hardver
struct SwapChainSupportDetails {
    //A képernyő/ablak korlátai (pl. min/max képszám, felbontás határai)
    VkSurfaceCapabilitiesKHR capabilities;
    //Támogatott pixelformátumok és színterek (pl. 8-bites RGB, HDR)
    std::vector<VkSurfaceFormatKHR> formats;
    //milyen modon kezelik ezeket ak képeket mikor jelenítik meg
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    // A konstruktor elkéri az ablakot, hogy tudjon hozzá Felszínt (Surface) csinálni
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    // -- Publikus változók (A többi osztálynak szüksége lesz rájuk) --
    //maga vulkan player
    VkInstance instance;
    //hibakezelő elérése
    VkDebugUtilsMessengerEXT debugMessenger;
    //A Vulkan-kompatibilis vászon, amit ráfeszítünk az ablakra
    VkSurfaceKHR surface;
    //ez maga a gpu
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    //A logikai vezérlőnk (ezen keresztül adunk ki minden parancsot a GPU-nak)
    VkDevice device;
    //ahova a 3D rajzolási parancsokat küldjük
    VkQueue graphicsQueue;
    //A sor, ahova a "Tedd ki a képernyőre!" parancsokat küldjük
    VkQueue presentQueue;        

    //swapchain példány
    VkSwapchainKHR swapChain;
    //a képek amik a swapchain csinál
    std::vector<VkImage> swapChainImages;
    //a swapchain képek beállításai
    VkFormat swapChainImageFormat;
    //swapchain kép mérete
    VkExtent2D swapChainExtent;
    //swapchan képeihez való lencse amin keresztül látjuk őket
    std::vector<VkImageView> swapChainImageViews;

private:
    GLFWwindow* window; // <--- Ide mentjük el az ablakot

    // -- Inicializáló függvények (Csak a konstruktor hívja őket) --
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();

    // -- Segédfüggvények --
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};