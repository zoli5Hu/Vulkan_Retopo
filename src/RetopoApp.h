#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>
#include "VulkanContext.h"
#include "ResourceManager.h"
#include "Renderer.h"      // <--- BEKÖTÖTTÜK A RENDERERT
#include "Pipeline.h"
#include "Vertex.h"

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class RetopoApp {
public:
    void run();

private:
    const int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // --- A Három Fő Modulunk! ---
    std::unique_ptr<VulkanContext> vulkanContext;
    std::unique_ptr<ResourceManager> resourceManager;
    std::unique_ptr<Renderer> renderer;             // <--- ÚJ: A RAJZOLÓNK

    GLFWwindow* window;

    // --- Erőforrások (Amit mi "birtokolunk") ---
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    std::unique_ptr<Pipeline> graphicsPipeline;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // --- Függvények ---
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentFrame);
};