#include "RetopoApp.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

//képernyő megadás
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// Bekapcsoljuk a Khronos hivatalos hibakereső rétegét
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

//ha debug modban vagyunk akkor true
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

void RetopoApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void RetopoApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "3D Retopo Tool", nullptr, nullptr);
}

void RetopoApp::initVulkan() {
    createInstance();
}

void RetopoApp::mainLoop() {
    //program ablak folyamatos futása a be nem zárásig itt fogjuk pl a frameket rajzolni
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void RetopoApp::cleanup() {
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void RetopoApp::createInstance() {
    //ha debug modban vagyunk
    if (enableValidationLayers) {
        std::cout << "Validation layerek bekapcsolva!" << std::endl;
    }

    //alap beállítások {} minden nullptr kivév amit beállítok
    //az appliction | instance között az a külömbség
    //az ap az struktúra az instance egy példánya aminek értékül adjuk
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Retopo App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // --- 1. KITERJESZTÉSEK BEÁLLÍTÁSA (Multiplatform módra) ---
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // MAC SPECIFIKUS: Csak akkor adjuk hozzá a MoltenVK kiterjesztést, ha Apple gépen fordul a kód
    #ifdef __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        //megfelelő bit bekapcsolása bitmask
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // --- 2. VALIDATION LAYEREK BEÁLLÍTÁSA ---
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // --- 3. VULKAN INSTANCE LÉTREHOZÁSA ---
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vulkan Instance-t!");
    }

    std::cout << "Vulkan Instance sikeresen letrehozva!" << std::endl;
}