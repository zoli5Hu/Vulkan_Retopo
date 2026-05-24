#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanContext.h"
#include "Pipeline.h"

class Renderer {
public:
    Renderer(VulkanContext* vulkanContext, VkImageView depthImageView, VkFormat depthFormat);
    ~Renderer();

    // Getterek, amiket a RetopoApp használni fog
    VkRenderPass getRenderPass() const { return renderPass; }
    uint32_t getCurrentFrame() const { return currentFrame; }

    // A fő rajzoló függvény
    void drawFrame(Pipeline* pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount, VkDescriptorSet descriptorSet);

private:
    VulkanContext* vulkanContext;
    VkImageView depthImageView;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // Építő függvények
    void createRenderPass(VkFormat depthFormat);
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Pipeline* pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount, VkDescriptorSet descriptorSet);
};