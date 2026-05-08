#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class Pipeline {
public:
    // A konstruktor egyből betölti a shadereket és megépíti a futószalagot
    Pipeline(VkDevice device, const std::string& vertFilepath, const std::string& fragFilepath);
    ~Pipeline();

private:
    VkDevice device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Segédfüggvény a bináris .spv fájlok beolvasásához
    static std::vector<char> readFile(const std::string& filepath);

    // A fő építő függvény
    void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath);

    // Csomagoló függvény a Vulkan Shader Modulokhoz
    VkShaderModule createShaderModule(const std::vector<char>& code);
};