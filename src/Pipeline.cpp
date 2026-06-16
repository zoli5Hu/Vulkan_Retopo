#include "Pipeline.h"
#include "Vertex.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

/**
 * @brief Inicializálja a grafikus futószalagot (Graphics Pipeline).
 * * A Pipeline határozza meg a Vulkan számára a teljes rajzolási folyamatot:
 * Bemeneti adatok (Vertex) -> Vertex Shader -> Primitívek (Háromszögelés) ->
 * Raszterizálás -> Fragment Shader (Színezés) -> Depth Test -> Framebuffer.
 */
Pipeline::Pipeline(VkDevice device, const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, VkExtent2D extent, VkDescriptorSetLayout descriptorSetLayout)
    : device(device) {
    createGraphicsPipeline(vertFilepath, fragFilepath, renderPass, extent, descriptorSetLayout);
}

/**
 * @brief Destruktor: Felszabadítja a Pipeline által lefoglalt GPU erőforrásokat.
 * A törlés a létrehozással ellentétes sorrendben történik, közvetlenül a logikai eszközön (device).
 */
Pipeline::~Pipeline() {
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

/**
 * @brief Felépíti a fix funkciójú és a programozható pipeline szakaszokat.
 */
void Pipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, VkRenderPass renderPass, VkExtent2D extent, VkDescriptorSetLayout descriptorSetLayout) {

    // Shader binárisok (SPIR-V) beolvasása a fájlrendszerből
    auto vertShaderCode = readFile(vertFilepath);
    auto fragShaderCode = readFile(fragFilepath);

    // Shader modulok létrehozása a beolvasott bájtkódból
    vertShaderModule = createShaderModule(vertShaderCode);
    fragShaderModule = createShaderModule(fragShaderCode);

    // =====================================================================
    // 1. Programozható Shader Szakaszok (Shader Stages)
    // =====================================================================
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main"; // A belépési pont a shader kódban

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // =====================================================================
    // 2. Vertex Input (Bemeneti adatok szerkezete)
    // =====================================================================
    // Leírja a GPU-nak a bejövő Vertex adatok fizikai memóriakiosztását
    // (Lépésköz, memóriacímek, adatformátumok).
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // =====================================================================
    // 3. Input Assembly (Geometriai Topológia)
    // =====================================================================
    // Meghatározza, hogyan építsen a GPU alakzatot a pontokból (itt: Háromszög-lista).
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // =====================================================================
    // 4. Viewport és Scissor (Nézet és Vágás)
    // =====================================================================
    // Viewport: Hova és mekkora méretben skálázódjon a renderelt kép az ablakon belül.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) extent.width;
    viewport.height = (float) extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor: Levágja a képet, ha a renderelés túllógna a megadott téglalapon.
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // =====================================================================
    // 5. Raszterizáló (Rasterization)
    // =====================================================================
    // A matematikai geometriát (vektorokat) alakítja képernyőpixelekké.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Lehetne LINE (Drótváz) is
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;       // Nincs hátlap-eldobás (Culling)
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // =====================================================================
    // 6. Multisampling (Élsimítás)
    // =====================================================================
    // Alapértelmezett állapot, jelenleg nincs élsimítás (1 minta/pixel).
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // =====================================================================
    // 7. Color Blending (Színkeverés és Átlátszóság)
    // =====================================================================
    // Meghatározza, hogyan keveredjen az új szín a már framebufferben lévő színnel.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // Kikapcsolva (Alpha blending nincs)

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // =====================================================================
    // 8. Depth / Stencil Test (Mélységi teszt / Z-Buffer)
    // =====================================================================
    // Gondoskodik arról, hogy a közelebbi 3D objektumok eltakarják a távolabbiakat.
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;           // Mélység ellenőrzése
    depthStencil.depthWriteEnable = VK_TRUE;          // Mélységi adatok frissítése
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // Csak akkor rajzol, ha közelebb van
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // =====================================================================
    // 9. Pipeline Layout (Erőforrás Kötések Tervrajza)
    // =====================================================================
    // Összeköti a shader programokat a CPU-ról érkező Uniform Bufferekkel (UBO) és textúrákkal.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Pipeline Layout letrehozasa sikertelen!");
    }

    // =====================================================================
    // 10. A Grafikus Pipeline Összeszerelése (Végső objektum)
    // =====================================================================
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Graphics Pipeline letrehozasa sikertelen!");
    }

    std::cout << "Graphics Pipeline (A Tervrajz) sikeresen megepitve!" << std::endl;
}

/**
 * @brief Bináris fájl beolvasó segédfüggvény a lefordított SPIR-V shaderekhez.
 * @param filepath A shader fájl elérési útja (pl. "shaders/vert.spv").
 * @return A fájl tartalma bájt-tömbként (vector<char>).
 */
std::vector<char> Pipeline::readFile(const std::string& filepath) {
    // Fájl megnyitása bináris módban, a mutatót a fájl végére rakjuk (ate = at end),
    // így egyből megkapjuk a méretét.
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Hiba: Nem sikerult megnyitni a shader fajlt: " + filepath);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    // Visszaugrunk a fájl elejére és beolvassuk az egészet a bufferbe
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

/**
 * @brief Becsomagolja a nyers SPIR-V bájtkódot egy Vulkan Shader Modulba.
 * Ezt a modult fogja a Vulkan Pipeline a futószalaghoz kapcsolni.
 */
VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();

    // A Vulkan a bináris kódot uint32_t mutatóként várja (memory alignment miatt)
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Shader Modult!");
    }

    return shaderModule;
}