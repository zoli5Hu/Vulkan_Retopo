#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>
#include "VulkanContext.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "Pipeline.h"
#include "Vertex.h"


//MVP mátrixok: A modell térbeli helyzete (Model),
//a kamera pozíciója (View) és a lencse látószöge/perspektívája
//(Proj)
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class RetopoApp {
public:
    void run();

private:
    //az előkészített képek száma
    const int MAX_FRAMES_IN_FLIGHT = 2;

    //vertex adatok objből
    std::vector<Vertex> vertices;
    //index buffer hogy a vertexek ne duplikáltan legyenek benne
    std::vector<uint32_t> indices;

    // --- A Három Fő Modulunk! ---
    //komunikációt teszi lehetővé a vulkan apival
    std::unique_ptr<VulkanContext> vulkanContext;
    //Modellek betöltése és a fizikai GPU memória (VRAM) lefoglalása/kezelése
    std::unique_ptr<ResourceManager> resourceManager;
    //render indítása
    std::unique_ptr<Renderer> renderer;

    //maga az ablakra mutató pointer
    GLFWwindow* window;

    // --- Erőforrások (Amit mi "birtokolunk") ---
    //mélységi kép hogy tudjuk mi van elől hátul
    VkImage depthImage;
    //mélységi adatok memória területe
    VkDeviceMemory depthImageMemory;
    //a lencse amivel látjuk az adatokat
    VkImageView depthImageView;

    //shader futószalag adatai
    std::unique_ptr<Pipeline> graphicsPipeline;

    // A logikai tartály (doboz), ami a vertexeket (3D pontokat) fogja tartalmazni
    VkBuffer vertexBuffer;
    // A ténylegesen lefoglalt fizikai videómemória (VRAM), ami a fenti tartályhoz tartozik
    VkDeviceMemory vertexBufferMemory;

    // A logikai tartály, ami a csúcspontok összekötési sorrendjét (indexeket) tartalmazza
    VkBuffer indexBuffer;
    // A tényleges fizikai videómemória (VRAM) az indexek számára
    VkDeviceMemory indexBufferMemory;
    //shade szabványa a binding alapján
    VkDescriptorSetLayout descriptorSetLayout;
    //description setek lefoglal terület a többinek
    VkDescriptorPool descriptorPool;
    //memória címek a bufferre
    std::vector<VkDescriptorSet> descriptorSets;

    //az a memória terület amit mindig felülírunk frissített adatokkal
    std::vector<VkBuffer> uniformBuffers;
    //azok a területek amiket felül írunk
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    //memória direkt felülírása
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