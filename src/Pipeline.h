#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

/**
 * @class Pipeline
 * @brief A Vulkan grafikus futószalagját (Graphics Pipeline) felépítő és kezelő osztály.
 * * Ez az osztály felelős a shader fájlok (.spv) beolvasásáért, a bemeneti adatok (Vertex)
 * specifikálásáért, a Viewport/Scissor, valamint a mélységi tesztelés (Z-buffer) beállításáért.
 * Végeredményben egy komplex Vulkan Pipeline objektumot állít elő a hardver számára.
 */
class Pipeline {
public:
    /**
     * @brief Konstruktor, amely azonnal betölti a shadereket és megépíti a teljes futószalagot.
     * * @param device A Vulkan logikai eszköz (Logical Device), amely a hardverrel kommunikál.
     * @param vertFilepath A lefordított Vertex Shader (.spv) relatív elérési útja.
     * @param fragFilepath A lefordított Fragment Shader (.spv) relatív elérési útja.
     * @param renderPass A megjelenítési fázis (Render Pass), amelybe ez a futószalag illeszkedik.
     * @param extent A Swapchain (vászon) aktuális felbontása (szélesség és magasság).
     * @param descriptorSetLayout A Uniform Buffer-ek (UBO) és külső adatok memóriakiosztási terve.
     */
    Pipeline(VkDevice device,
             const std::string& vertFilepath,
             const std::string& fragFilepath,
             VkRenderPass renderPass,
             VkExtent2D extent,
             VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Destruktor, amely biztonságosan felszabadítja a Vulkan Pipeline erőforrásait a memóriából.
     */
    ~Pipeline();

    /**
     * @brief Visszaadja a megépített grafikus futószalag Vulkan handle-jét.
     * Ezt használja a Renderer osztály a rajzolási parancsok rögzítésekor (vkCmdBindPipeline).
     * @return VkPipeline A tényleges grafikus futószalag.
     */
    VkPipeline getPipeline() const { return graphicsPipeline; }

    /**
     * @brief Visszaadja a Pipeline elrendezését (Layout).
     * Szükséges a Descriptor Set-ek (Uniform Bufferek) shaderhez való csatlakoztatásakor.
     * @return VkPipelineLayout A futószalag elrendezése.
     */
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    /** @brief Referencia a Vulkan logikai eszközre. A törlési műveletekhez elengedhetetlen. */
    VkDevice device;

    /** @brief A Vulkan grafikus futószalag fő objektuma, amit a hardver használni fog. */
    VkPipeline graphicsPipeline;

    /** @brief A futószalag "csatlakozási felülete", ami megmondja a GPU-nak, milyen adatok (UBO) érkeznek. */
    VkPipelineLayout pipelineLayout;

    /** @brief A betöltött és GPU memóriába becsomagolt Vertex Shader. */
    VkShaderModule vertShaderModule;

    /** @brief A betöltött és GPU memóriába becsomagolt Fragment Shader. */
    VkShaderModule fragShaderModule;

    /**
     * @brief Belső segédfüggvény a lefordított SPIR-V bináris shader fájlok biztonságos beolvasására.
     * * @param filepath A bináris fájl elérési útja.
     * @return std::vector<char> A fájl nyers, bájt-szintű tartalma.
     */
    static std::vector<char> readFile(const std::string& filepath);

    /**
     * @brief Végrehajtja a Pipeline 10 különböző szakaszának paraméterezését és a Vulkan objektum generálását.
     * Ez a függvény rakja össze a Vertex Input, Assembly, Viewport, Raszterizáló és Z-Buffer fázisokat.
     */
    void createGraphicsPipeline(const std::string& vertFilepath,
                                const std::string& fragFilepath,
                                VkRenderPass renderPass,
                                VkExtent2D extent,
                                VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief A nyers bináris kódsorból Vulkan Shader Modult hoz létre.
     * * @param code A readFile függvény által visszaadott bájtkód.
     * @return VkShaderModule A Vulkan által értelmezhető és futtatható shader modul.
     */
    VkShaderModule createShaderModule(const std::vector<char>& code);
};