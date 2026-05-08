#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <memory>
#include "Pipeline.h"     

// Ebbe gyüjtjük össze a parancs családokat
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily; // A megjelenítésért felelős részleg

    // Akkor teljes, ha tud grafikát számolni ÉS meg is tudja jeleníteni
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// Ebbe a csomagba gyűjtjük össze a monitor/ablak képességeit
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class RetopoApp {
public:
    void run();

private:
    GLFWwindow* window;

    //ennek mondom meg az alkalmazés
    // adatait
    // layereket
    // extensionokat
    VkInstance instance;
    VkSurfaceKHR surface;        // A vászon, ami összeköti a Vulkant az ablakkal

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;        // A csatorna, ami kiküldi a képet az ablakra

    VkSwapchainKHR swapChain;                   // Maga a csere-lánc objektum
    std::vector<VkImage> swapChainImages;       // A vásznak (képek) amikre rajzolunk majd
    VkFormat swapChainImageFormat;              // A kiválasztott színformátum
    VkExtent2D swapChainExtent;                 // A vásznak tényleges felbontása (szélesség, magasság)

    // ---  VÁLTOZÓ: A képek "lencséi" ---
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;

    VkDebugUtilsMessengerEXT debugMessenger;
    std::unique_ptr<Pipeline> graphicsPipeline;


    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void createInstance();

    // ---  FÜGGVÉNYEK ---
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void createSurface();

    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void createLogicalDevice();

    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers(); 

    // Segédfüggvények a Swap Chain beállításához:
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

};