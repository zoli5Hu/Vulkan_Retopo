#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class RetopoApp {
public:
    void run();

private:
    GLFWwindow* window;

    //ennek mondom meg az alkalmazés
    // adatait
    // layereket
    // extensionokat
    VkInstance instance;

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void createInstance();
};