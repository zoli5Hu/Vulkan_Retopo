#include "RetopoApp.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <set> // Új könyvtár a duplikációk elkerüléséhez

//képernyő megadás
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// Bekapcsoljuk a Khronos hivatalos hibakereső rétegét
const std::vector<const char *> validationLayers = {
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
    createSurface(); // <-- Felszín létrehozása a kártya választás ELŐTT
    pickPhysicalDevice();
    createLogicalDevice(); // Létrehozzuk a logikai eszközt
}

void RetopoApp::mainLoop() {
    //program ablak folyamatos futása a be nem zárásig itt fogjuk pl a frameket rajzolni
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void RetopoApp::cleanup() {
    // Logikai eszköz törlése (először ezt töröljük, mert ez függ az instance-tól)
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr); // Felszín törlése
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
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

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

// --- ÚJ FÜGGVÉNY: Felszín létrehozása ---
void RetopoApp::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Window Surface-t!");
    }
    std::cout << "Window Surface sikeresen letrehozva!" << std::endl;
}

void RetopoApp::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Hiba: Nem talalhato Vulkan kompatibilis videokartya!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device: devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Hiba: Nem talalhato a feladatra alkalmas videokartya!");
    }
}

bool RetopoApp::isDeviceSuitable(VkPhysicalDevice device) {
    // Lekérdezzük a kártya tulajdonságait (pl. név, típus)
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    QueueFamilyIndices indices = findQueueFamilies(device);

    // Később itt fogjuk ellenőrizni, hogy a kártya tud-e pl. geometriai shadereket (retopohoz hasznos),
    // de egyelőre bármilyen Vulkan-képes kártyát elfogadunk.
    std::cout << "Megtalalt GPU: " << deviceProperties.deviceName << std::endl;

    return indices.isComplete();
}

//videókártya részlegének kiválasztásai
QueueFamilyIndices RetopoApp::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily: queueFamilies) {
        // 1. Tud rajzolni?
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // 2. ÚJ: Tudja kezelni az ablakunkat (Surface)?
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

//ez felel a komunikációért a gpuval
void RetopoApp::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // Itt átadjuk az összes szükséges Queue-t (ha 1 db, ha 2 db)
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;

    // MAC SPECIFIKUS: Ha Apple gépen vagyunk, kötelező a portability subset a logikai eszközhöz is
#ifdef __APPLE__
    const std::vector<const char *> deviceExtensions = {
        "VK_KHR_portability_subset"
    };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
#endif

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a logikai eszközt!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue); //  Kép megjelenítő futószalag
}