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
             VkExtent2D extent,
             VkDescriptorSetLayout descriptorSetLayout);

    ~Pipeline();

    // Hogy a RetopoApp el tudja kérni a megépített futószalagot ---
    VkPipeline getPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
private:
    //ez komunikál a hardverrel
    VkDevice device;
    //ez a telejes futószalag, amit a Renderer használni fog
    VkPipeline graphicsPipeline;
    //layout binding használata
    VkPipelineLayout pipelineLayout;
    //ver,frag becsomagolva
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Segédfüggvény a bináris .spv fájlok beolvasásához
    static std::vector<char> readFile(const std::string& filepath);

    // A fő építő függvény
    // Frissített építő függvény
    void createGraphicsPipeline(const std::string& vertFilepath,
                                const std::string& fragFilepath,
                                VkRenderPass renderPass,
                                VkExtent2D extent,
                                VkDescriptorSetLayout descriptorSetLayout); // <--- EZ HIÁNYZOTT INNEN!


    // Csomagoló függvény a Vulkan Shader Modulokhoz
    VkShaderModule createShaderModule(const std::vector<char>& code);


};