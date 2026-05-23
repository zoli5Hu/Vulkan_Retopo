#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <memory>
#include "Pipeline.h"
#include "Vertex.h"

// Ebbe gyüjtjük össze a parancs családokat...


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

// --- ÚJ VÁLTOZÓK: A 3D Kamera (Uniform Bufferek) ---
struct UniformBufferObject {
    glm::mat4 model; // Forgatás és pozíció
    glm::mat4 view;  // Kamera nézete
    glm::mat4 proj;  // Perspektíva
};



class RetopoApp {
public:
    void run();

private:
    const int MAX_FRAMES_IN_FLIGHT = 2;
    // A régi hardkódolt tömb helyett most üres vektorok várják az obj fájl adatait:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices; // <-- ÚJ: Az Index lista (összeköttetések)




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

    // --- ÚJ VÁLTOZÓK: Mélység-tároló (Z-Buffer) ---
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // --- ÚJ VÁLTOZÓK ---
    VkCommandPool commandPool;

    // --- ÚJ VÁLTOZÓK: A szinkronizációs lámpák ---
    std::vector<VkCommandBuffer> commandBuffers; // Többes szám!
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0; // Nyilvántartja, melyik "szett" lámpát használjuk épp

    VkRenderPass renderPass;

    VkDebugUtilsMessengerEXT debugMessenger;
    std::unique_ptr<Pipeline> graphicsPipeline;

    // --- ÚJ VÁLTOZÓK: Vertex Buffer (GPU Memória) ---
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;             // <-- ÚJ
    VkDeviceMemory indexBufferMemory; // <-- ÚJ

    VkDescriptorSetLayout descriptorSetLayout; // A "leírás", amit átadunk a Pipeline-nak
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // Minden képkockának (2 db) saját kamera memóriája lesz, hogy ne akadjanak össze
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;


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
    void createCommandPool();    // <--- ÚJ FÜGGVÉNY
    void createCommandBuffer();  // <--- ÚJ FÜGGVÉNY
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();
    void drawFrame();

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentFrame);

    // --- ÚJ FÜGGVÉNYEK: Mélység-tárolóhoz és képalkotáshoz ---
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Segédfüggvények a Swap Chain beállításához:
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

};