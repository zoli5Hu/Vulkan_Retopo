#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

// Egy kis segédstruktúra, ami tárolja, hogy megvan-e a grafikus sor a kártyán
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    // Akkor teljes, ha találtunk grafikus sort
    bool isComplete() {
        return graphicsFamily.has_value();
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

    // A kiválasztott videókártya (ezt a Vulkan magától megsemmisíti, nem kell a cleanup-ba tenni)
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    //queue family meghatározása
    VkDevice device;             // A logikai eszköz (a szerződés a kártyával)
    VkQueue graphicsQueue;       // A csatorna, amin a grafikus parancsokat küldjük

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void createInstance();
    //GPU kiválasztás
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);

    // --- ÚJ FÜGGVÉNYEK ---
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void createLogicalDevice();
};