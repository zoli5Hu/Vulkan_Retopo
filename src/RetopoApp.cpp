#include "RetopoApp.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

void RetopoApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void RetopoApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "3D Retopo Tool", nullptr, nullptr);
}

void RetopoApp::initVulkan() {
    // 1. ALAPOZÁS (Ablak és Vulkan)
    vulkanContext = std::make_unique<VulkanContext>(window);

    // 2. ERŐFORRÁS MENEDZSER
    resourceManager = std::make_unique<ResourceManager>(vulkanContext->device, vulkanContext->physicalDevice);

    // 3. Z-BUFFER KÉSZÍTÉSE
    VkFormat depthFormat = resourceManager->findDepthFormat();
    resourceManager->createDepthResources(vulkanContext->swapChainExtent, depthFormat, depthImage, depthImageMemory, depthImageView);

    // 4. RAJZOLÓ (Renderer) ELINDÍTÁSA
    renderer = std::make_unique<Renderer>(vulkanContext.get(), depthImageView, depthFormat);

    // 5. GEOMETRIA BETÖLTÉSE
    resourceManager->loadModel("modells/kocka.obj", vertices, indices);
    resourceManager->createVertexBuffer(vertexBuffer, vertexBufferMemory, vertices);
    resourceManager->createIndexBuffer(indexBuffer, indexBufferMemory, indices);

    // 6. KAMERA RENDSZER
    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // 7. SHADEREK ÉS TERVRAJZ (Pipeline)
    graphicsPipeline = std::make_unique<Pipeline>(
            vulkanContext->device,
            "shaders/vert.spv",
            "shaders/frag.spv",
            renderer->getRenderPass(),      // <--- Már a Renderertől kérjük el!
            vulkanContext->swapChainExtent,
            descriptorSetLayout
        );
}

void RetopoApp::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 1. Frissítjük a kamerát/forgást az adott képkockához
        updateUniformBuffer(renderer->getCurrentFrame());

        // 2. Szólunk a Renderernek, hogy rajzolja ki a kockát
        renderer->drawFrame(graphicsPipeline.get(), vertexBuffer, indexBuffer, static_cast<uint32_t>(indices.size()), descriptorSets[renderer->getCurrentFrame()]);
    }
    vkDeviceWaitIdle(vulkanContext->device);
}

void RetopoApp::cleanup() {
    // Kártya leállítása, mielőtt törlünk
    vkDeviceWaitIdle(vulkanContext->device);

    // 1. Birtokolt memóriánk törlése
    vkDestroyBuffer(vulkanContext->device, indexBuffer, nullptr);
    vkFreeMemory(vulkanContext->device, indexBufferMemory, nullptr);
    vkDestroyBuffer(vulkanContext->device, vertexBuffer, nullptr);
    vkFreeMemory(vulkanContext->device, vertexBufferMemory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanContext->device, uniformBuffers[i], nullptr);
        vkFreeMemory(vulkanContext->device, uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(vulkanContext->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, descriptorSetLayout, nullptr);

    vkDestroyImageView(vulkanContext->device, depthImageView, nullptr);
    vkDestroyImage(vulkanContext->device, depthImage, nullptr);
    vkFreeMemory(vulkanContext->device, depthImageMemory, nullptr);

    // 2. Modulok törlése KÖTELEZŐ SORRENDBEN (Tetőtől az alapokig)
    graphicsPipeline.reset();
    renderer.reset();        // <--- Itt törlődnek a framebufferek és a command pool automatikusan!
    resourceManager.reset();
    vulkanContext.reset();   // <--- Itt törlődik a device és az instance

    // 3. Ablak törlése
    glfwDestroyWindow(window);
    glfwTerminate();
}

void RetopoApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Set Layout-ot!");
    }
}

void RetopoApp::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(vulkanContext->device, &bufferInfo, nullptr, &uniformBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Uniform Buffert!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vulkanContext->device, uniformBuffers[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = resourceManager->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(vulkanContext->device, &allocInfo, nullptr, &uniformBuffersMemory[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Uniform Buffernek!");
        }

        vkBindBufferMemory(vulkanContext->device, uniformBuffers[i], uniformBuffersMemory[i], 0);

        vkMapMemory(vulkanContext->device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void RetopoApp::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(vulkanContext->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Pool-t!");
    }
}

void RetopoApp::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vulkanContext->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult lefoglalni a Descriptor Set-eket!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(vulkanContext->device, 1, &descriptorWrite, 0, nullptr);
    }
}

void RetopoApp::updateUniformBuffer(uint32_t currentFrame) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                vulkanContext->swapChainExtent.width / (float) vulkanContext->swapChainExtent.height,
                                0.1f, 10.0f);

    ubo.proj[1][1] *= -1; // A Vulkan Y-tengely korrekciója

    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}






