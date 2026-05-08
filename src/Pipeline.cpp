#include "Pipeline.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

Pipeline::Pipeline(VkDevice device, const std::string& vertFilepath, const std::string& fragFilepath) : device(device) {
    createGraphicsPipeline(vertFilepath, fragFilepath);
}

Pipeline::~Pipeline() {
    // A shader modulokra csak a futószalag megépítéséig van szükség, utána törölhetjük őket
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

// Bináris fájl beolvasó
std::vector<char> Pipeline::readFile(const std::string& filepath) {
    // ate = at the end (a fájl végére ugrik), binary = bináris formátum
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Hiba: Nem sikerult megnyitni a shader fajlt: " + filepath);
    }

    // Mivel a fájl végén állunk, a tellg() megadja a fájl méretét byte-okban
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    // Visszaugrunk a fájl elejére, és beolvassuk az egészet a vektorunkba
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void Pipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath) {
    // 1. Beolvassuk a bináris gépi kódot
    auto vertShaderCode = readFile(vertFilepath);
    auto fragShaderCode = readFile(fragFilepath);

    std::cout << "Vertex Shader betoltve! Meret: " << vertShaderCode.size() << " byte" << std::endl;
    std::cout << "Fragment Shader betoltve! Meret: " << fragShaderCode.size() << " byte" << std::endl;

    // 2. Vulkan Shader Modulokat csinálunk belőlük
    vertShaderModule = createShaderModule(vertShaderCode);
    fragShaderModule = createShaderModule(fragShaderCode);

    // Ide fog jönni a Graphics Pipeline többi beállítása a következő lépésekben!
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();

    // A Vulkan elvárja, hogy a bináris kód uint32_t formátumban legyen, ezért átkasztoljuk
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Shader Modult!");
    }

    return shaderModule;
}