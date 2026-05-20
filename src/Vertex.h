#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

struct Vertex {
    glm::vec3 pos;   // 3D Pozíció (x, y,z)
    glm::vec3 color; // Szín (r, g, b)

    // 1. Megmondja a Vulkannak, mekkora egy darab Vertex csomag a memóriában
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // 2. Megmondja a Vulkannak, hogy a csomagon belül hol találja a pozíciót és a színt
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Pozíció azonosítója
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        // <--- MÓDOSÍTÁS: R32G32-ből R32G32B32 lett (3 dimenzió)
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Szín azonosítója
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};