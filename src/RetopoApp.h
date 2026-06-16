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

/**
 * @struct UniformBufferObject
 * @brief A shader-eknek (GPU) minden képkockánál átküldött transzformációs mátrixok (MVP).
 * * Model: A 3D háló helyzete a világban.
 * * View: A kamera pozíciója és iránya (Blender stílusú nézet).
 * * Proj: A lencse perspektivikus torzítása (látószög, képarány).
 */
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

/**
 * @class RetopoApp
 * @brief Az alkalmazás fő vezérlőosztálya (Application Layer).
 * * Ez az osztály fogja össze az ablakkezelést (GLFW), a felhasználói felületet (ImGui),
 * a memóriakezelést (ResourceManager) és a megjelenítést (Renderer). Itt fut a fő
 * Game Loop, és itt kezeljük a felhasználói interakciókat (kamera, fájlmegnyitás).
 */
class RetopoApp {
public:
    /**
     * @brief Elindítja az alkalmazást. Inicializál mindent, futtatja a ciklust, majd a végén takarít.
     */
    void run();

private:
    /** @brief A párhuzamosan (Double Buffering) feldolgozott képkockák száma a zökkenőmentes megjelenítésért. */
    const int MAX_FRAMES_IN_FLIGHT = 2;

    /** @brief (Opcionális/Korábbi) A nyers betöltött pontok globális listája. */
    std::vector<Vertex> vertices;

    /** @brief A memóriába betöltött, rajzolásra kész 3D modellek (High Poly, Low Poly) listája. */
    std::vector<ModelData> loadedModels;

    // --- A Három Fő Modulunk (Okos mutatókkal, az automatikus memóriakezelésért) ---

    /** @brief A Vulkan API alaprendszerét (Instance, Device, Swapchain) felügyelő objektum. */
    std::unique_ptr<VulkanContext> vulkanContext;

    /** @brief Az OBJ fájlok beolvasásáért és a fizikai VRAM memóriafoglalásokért felelős modul. */
    std::unique_ptr<ResourceManager> resourceManager;

    /** @brief A tényleges grafikus futószalagot és rajzolási parancsokat (Command Buffers) vezérlő modul. */
    std::unique_ptr<Renderer> renderer;

    /** @brief A GLFW által biztosított platformfüggetlen operációs rendszeri ablak. */
    GLFWwindow* window;

    // --- Kamera és Egér Rendszer (Blender stílusú irányítás) ---
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f); ///< Mit nézünk (Fókuszpont, 3D origó)
    float cameraRadius = 4.0f;                            ///< Milyen messze vagyunk a fókuszponttól (Zoom)
    float cameraYaw = 45.0f;                              ///< Vízszintes forgás szöge (Egyenlítő mentén)
    float cameraPitch = 30.0f;                            ///< Függőleges forgás szöge (Észak-Déli irány)

    bool isOrbiting = false;                              ///< Aktív-e a kamera forgatása (Lenyomott gomb)
    double lastMouseX = 0.0;                              ///< Az egér előző X koordinátája a képernyőn
    double lastMouseY = 0.0;                              ///< Az egér előző Y koordinátája a képernyőn
    bool isPanning = false;                               ///< Aktív-e a nézet eltolása (Shift + Kattintás)

    // --- GLFW C-stílusú "lehallgató" (callback) függvényei ---
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // --- Belső C++ eseménykezelők, amelyek ténylegesen módosítják a kamera változókat ---
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double yoffset);

    // --- Vulkan Erőforrások (Grafikus elemek) ---
    VkImage depthImage;               ///< Mélységi textúra a Z-Bufferhez (Takarások számítása)
    VkDeviceMemory depthImageMemory;  ///< A Z-Bufferhez lefoglalt fizikai VRAM memória
    VkImageView depthImageView;       ///< A lencse, amin keresztül a GPU olvassa/írja a mélységet

    /** @brief A Vulkan grafikus futószalagja (Shaderek, bemeneti adatok, raszterizáló). */
    std::unique_ptr<Pipeline> graphicsPipeline;

    // --- (Korábbi verziós) Bufferek ---
    VkDeviceMemory vertexBufferMemory; ///< Ténylegesen lefoglalt fizikai VRAM a Vertexeknek
    VkBuffer indexBuffer;              ///< Logikai tartály a csúcspontok összekötési sorrendjének (Topológia)

    // --- Uniform Buffer (Kamera adatok) hivatkozásai ---
    VkDescriptorSetLayout descriptorSetLayout;     ///< A Shader adatbekötési szabványa
    VkDescriptorPool descriptorPool;               ///< Előre lefoglalt memóriaterület a Descriptor Set-eknek
    std::vector<VkDescriptorSet> descriptorSets;   ///< Konkrét mutatók a buffer memóriára

    std::vector<VkBuffer> uniformBuffers;          ///< Logikai pufferek az MVP mátrixoknak (Képkockánként 1)
    std::vector<VkDeviceMemory> uniformBuffersMemory; ///< Fizikai memória az MVP mátrixoknak
    std::vector<void*> uniformBuffersMapped;       ///< CPU által közvetlenül írható memóriatérkép

    // --- UI és Modellcsere változói ---
    char highPolyInput[256] = "modells/kocka.obj"; ///< (Opcionális) ImGui szövegmező puffer
    char lowPolyInput[256] = "modells/monkey.obj"; ///< (Opcionális) ImGui szövegmező puffer

    std::string highPolyName = "Nincs betoltve";   ///< A UI-on megjelenített aktív High Poly fájlnév
    std::string lowPolyName = "Nincs betoltve";    ///< A UI-on megjelenített aktív Low Poly fájlnév

    /**
     * @brief Intelligens modell betöltő, amely aszinkron és memóriaszivárgás-mentes cserét biztosít.
     * @param slot 0 = High Poly, 1 = Low Poly hely.
     * @param filepath Az újonnan betöltendő OBJ fájl.
     * @param nameTracker A UI-on frissítendő név változója.
     */
    void loadModelIntoSlot(int slot, const std::string& filepath, std::string& nameTracker);

    // --- Rendszer és Ciklus Függvények ---
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentFrame);

    /** @brief (Segédfüggvény) Egyszerű modell betöltés az inicializálás fázisában. */
    void loadNewModel(const std::string& filepath);

    // --- ImGui (Felhasználói felület) specifikus változók ---
    VkDescriptorPool imguiPool; ///< Az ImGui saját memóriamedencéje a textúrákhoz és betűtípusokhoz.
    void initImGui();           ///< Felépíti a Dear ImGui Vulkan backendjét.
};