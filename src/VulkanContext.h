#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

// Ezek a struktúrák átköltöztek ide a RetopoApp.h-ból!
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    // A konstruktor elkéri az ablakot, hogy tudjon hozzá Felszínt (Surface) csinálni
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    // -- Publikus változók (A többi osztálynak szüksége lesz rájuk) --
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;        
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;        

    VkSwapchainKHR swapChain;                   
    std::vector<VkImage> swapChainImages;       
    VkFormat swapChainImageFormat;              
    VkExtent2D swapChainExtent;                 
    std::vector<VkImageView> swapChainImageViews; // <--- EZ HIÁNYZOTT!

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