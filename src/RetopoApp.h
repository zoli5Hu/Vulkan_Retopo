#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily; // A megjelenítésért felelős részleg

    // Akkor teljes, ha tud grafikát számolni ÉS meg is tudja jeleníteni
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
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

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void createInstance();

    // --- ÚJ FÜGGVÉNY ---
    void createSurface();

    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void createLogicalDevice();
};