#include "Renderer.h"
#include <stdexcept>
#include <array>
#include <iostream>
#include <ostream>

Renderer::Renderer(VulkanContext* vulkanContext, VkImageView depthImageView, VkFormat depthFormat) {
    this->vulkanContext = vulkanContext;
    this->depthImageView = depthImageView;

    createRenderPass(depthFormat);
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vulkanContext->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanContext->device, inFlightFences[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
        vkDestroySemaphore(vulkanContext->device, renderFinishedSemaphores[i], nullptr);
    }

    vkDestroyCommandPool(vulkanContext->device, commandPool, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(vulkanContext->device, framebuffer, nullptr);
    }

    vkDestroyRenderPass(vulkanContext->device, renderPass, nullptr);
}

void Renderer::createRenderPass(VkFormat depthFormat) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vulkanContext->swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(vulkanContext->device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Render Pass-t!");
    }
}

void Renderer::createFramebuffers() {
    swapChainFramebuffers.resize(vulkanContext->swapChainImageViews.size());

    for (size_t i = 0; i < vulkanContext->swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            vulkanContext->swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = vulkanContext->swapChainExtent.width;
        framebufferInfo.height = vulkanContext->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanContext->device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Framebuffert!");
        }
    }
}

void Renderer::createCommandPool() {
    // Biztonságos Queue Family keresés
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
            break;
        }
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vulkanContext->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Pool-t!");
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(vulkanContext->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Buffereket!");
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Pipeline* pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount, VkDescriptorSet descriptorSet) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult elkezdeni a Command Buffer rogziteset!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = vulkanContext->swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // A Kamera (DescriptorSet) bekötése
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    // Rajzolás!
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult befejezni a Command Buffer rogziteset!");
    }
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(vulkanContext->swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext->device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a szinkronizacios objektumokat!");
        }
    }

    for (size_t i = 0; i < vulkanContext->swapChainImages.size(); i++) {
        if (vkCreateSemaphore(vulkanContext->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a renderFinished semaphoret!");
        }
    }
}

void Renderer::drawFrame(Pipeline *pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indexCount,
                         VkDescriptorSet descriptorSet) {
    vkWaitForFences(vulkanContext->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vulkanContext->device, 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(vulkanContext->device, vulkanContext->swapChain, UINT64_MAX,
                          imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    // Itt történik a varázslat: ráküldjük az összes memóriát és a csomagot a teherautóra
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, pipeline, vertexBuffer, indexBuffer, indexCount,
                        descriptorSet);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vulkanContext->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult bekuldeni a rajzolo parancsot!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {vulkanContext->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(vulkanContext->presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
