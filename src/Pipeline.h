#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class Pipeline {
public:
    // A konstruktor egyből betölti a shadereket és megépíti a futószalagot
    // Módosított konstruktor: most már kéri a renderPass-t is!
    Pipeline(VkDevice device,
             const std::string& vertFilepath,
             const std::string& fragFilepath,
             VkRenderPass renderPass,
             VkExtent2D extent);
    ~Pipeline();

private:
    VkDevice device;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout; // Ez kell a shader változókhoz (később)
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Segédfüggvény a bináris .spv fájlok beolvasásához
    static std::vector<char> readFile(const std::string& filepath);

    // A fő építő függvény
    // Frissített építő függvény
    void createGraphicsPipeline(const std::string& vertFilepath,
                                const std::string& fragFilepath,
                                VkRenderPass renderPass,
                                VkExtent2D extent);
    
    // Csomagoló függvény a Vulkan Shader Modulokhoz
    VkShaderModule createShaderModule(const std::vector<char>& code);
};