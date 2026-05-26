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

    // --- ÚJ: Kamera és Egér (Blender stílus) ---
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f); // Mit nézünk (Fókuszpont)
    float cameraRadius = 4.0f;                            // Milyen messze vagyunk a tárgytól
    float cameraYaw = 45.0f;                              // Vízszintes forgás (Egyenlítő)
    float cameraPitch = 30.0f;                            // Függőleges forgás (Észak-Dél)

    bool isOrbiting = false;                              // Épp nyomva tartjuk-e a gombot
    double lastMouseX = 0.0;                              // Hol volt az egér az előző pillanatban
    double lastMouseY = 0.0;
    bool isPanning = false;                              // Épp pánikálunk-e (elmozdítjuk a kamerát)

    // GLFW C-stílusú "lehallgató" (callback) függvényei
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Belső C++ feldolgozók, amik már hozzáférnek az osztály változóihoz
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double yoffset);
    // ------------------------------------------


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