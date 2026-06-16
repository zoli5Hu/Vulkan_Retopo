#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanContext.h"
#include "Pipeline.h"
#include "ResourceManager.h" // Szükséges a ModelData struktúra eléréséhez

/**
 * @class Renderer
 * @brief A Vulkan grafikus megjelenítő motor magja.
 * * Ez az osztály felelős a tényleges rajzolási folyamatért. Kezeli a Render Pass-t,
 * a Framebuffereket, a Parancslistákat (Command Buffers), valamint a GPU és a CPU
 * közötti aszinkron kommunikációt biztosító szinkronizációs objektumokat (Fences, Semaphores).
 */
class Renderer {
public:
    /**
     * @brief Konstruktor, amely felépíti a rajzolási infrastruktúrát.
     * @param vulkanContext Mutató a Vulkan alaprendszerére (eszköz, Swapchain, stb.).
     * @param depthImageView A Z-Bufferhez (mélységteszt) tartozó képnézet.
     * @param depthFormat A Z-Buffer memóriába mentett formátuma (pl. VK_FORMAT_D32_SFLOAT).
     */
    Renderer(VulkanContext* vulkanContext, VkImageView depthImageView, VkFormat depthFormat);

    /**
     * @brief Destruktor, amely biztonságosan és a megfelelő sorrendben megsemmisíti a Vulkan objektumokat.
     */
    ~Renderer();

    /**
     * @brief Visszaadja a Render Pass (Megjelenítési Terv) Vulkan handle-jét.
     * Erre a Pipeline osztálynak van szüksége az építkezéskor.
     * @return VkRenderPass A grafikus futószalaghoz kapcsolódó Render Pass.
     */
    VkRenderPass getRenderPass() const { return renderPass; }

    /**
     * @brief Visszaadja a jelenleg feldolgozás alatt álló képkocka indexét (0 vagy 1).
     * @return uint32_t Az aktuális "in-flight" képkocka sorszáma.
     */
    uint32_t getCurrentFrame() const { return currentFrame; }

    /**
     * @brief A fő rajzoló metódus, amelyet másodpercenként 60-szor (vagy többször) hívunk meg.
     * Szinkronizálja a képkockákat, rögzíti a parancsokat, és beküldi a GPU-nak rajzolásra.
     * @param pipeline A grafikus futószalag, amely megmondja, *hogyan* rajzoljon a hardver.
     * @param models A memóriába betöltött 3D modellek listája (High Poly, Low Poly).
     * @param descriptorSet A kamera transzformációs mátrixait (UBO) tartalmazó memóriablokk.
     */
    void drawFrame(Pipeline* pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet);

private:
    /** @brief Referencia az alapvető Vulkan eszközökre. */
    VulkanContext* vulkanContext;

    /** @brief A mélységi teszteléshez használt 2D textúra nézete. */
    VkImageView depthImageView;

    /** @brief A párhuzamosan feldolgozott képkockák maximális száma (Double Buffering). */
    const int MAX_FRAMES_IN_FLIGHT = 2;

    /** @brief A jelenleg aktív képkocka indexe (0 vagy 1). */
    uint32_t currentFrame = 0;

    /** @brief A rajzolási fázisokat és csatolmányokat (Attachments) leíró objektum. */
    VkRenderPass renderPass;

    /** @brief A Swapchain képeihez és a Z-Bufferhez tartozó rajz-célpontok tartályai. */
    std::vector<VkFramebuffer> swapChainFramebuffers;

    /** @brief Memóriafoglalási medence a Command Bufferek számára a kártyán. */
    VkCommandPool commandPool;

    /** @brief A rajzolási utasításokat tároló listák (minden in-flight frame-hez egy). */
    std::vector<VkCommandBuffer> commandBuffers;

    // --- Szinkronizációs objektumok ---

    /** @brief Jelzi a rajzolónak, hogy a Swapchain-ből megérkezett a szabad kép, lehet rá rajzolni. */
    std::vector<VkSemaphore> imageAvailableSemaphores;

    /** @brief Jelzi a képernyőnek, hogy a rajzolás befejeződött, a kép kitehető a monitorra. */
    std::vector<VkSemaphore> renderFinishedSemaphores;

    /** @brief Blokkolja a CPU-t, hogy ne küldjön be új képkockát, amíg a GPU nem végzett a korábbival. */
    std::vector<VkFence> inFlightFences;

    // --- Építő és belső segédfüggvények ---

    void createRenderPass(VkFormat depthFormat);
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    /**
     * @brief A tényleges GPU parancsok (Draw Calls, Pipeline csatlakoztatás, ImGui) rögzítése.
     */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Pipeline* pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet);
};