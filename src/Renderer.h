#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanContext.h"
#include "Pipeline.h"
#include "ResourceManager.h" // <-- ÚJ: Ezt be kell húzni, hogy lássa a ModelData-t!

class Renderer {
public:
    Renderer(VulkanContext* vulkanContext, VkImageView depthImageView, VkFormat depthFormat);
    ~Renderer();

    // Getterek, amiket a RetopoApp használni fog
    VkRenderPass getRenderPass() const { return renderPass; }
    uint32_t getCurrentFrame() const { return currentFrame; }

    // MÓDOSÍTVA: A régi vertex/index bufferek helyett most a models listát kéri!
    void drawFrame(Pipeline* pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet);

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

    // MÓDOSÍTVA: Itt is a models listát kéri!
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Pipeline* pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet);
};