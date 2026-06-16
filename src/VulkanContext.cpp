#include "VulkanContext.h"
#include <iostream>
#include <set>
#include <algorithm> // A std::clamp miatt kell
#include <limits>    // A std::numeric_limits miatt kell
#include <stdexcept> // A std::runtime_error miatt kell

#include "VulkanUtils.h"

/**
 * @brief Inicializálja a Vulkan alaprendszerét és felépíti a hardveres kapcsolatot.
 * A konstruktor meghívásakor az összes kritikus Vulkan objektum (Instance, Device, Swapchain)
 * automatikusan létrejön a megfelelő sorrendben.
 * @param window Mutató a GLFW ablakra, amelyre a Vulkan rajzolni fog.
 */
VulkanContext::VulkanContext(GLFWwindow *window) {
    this->window = window;

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

VulkanContext::~VulkanContext() {}

// ============================================================================
// DEBUG ÉS VALIDÁCIÓS RÉTEGEK (Validation Layers)
// ============================================================================

// A validációs rétegek csak Debug módban aktívak (sebességoptimalizáció miatt Release-ben kikapcsolnak)
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

/**
 * @brief Proxy függvény a Debug Messenger létrehozásához.
 * Mivel ez egy kiterjesztés (Extension), a függvény mutatóját manuálisan kell lekérni a Vulkan-tól.
 */
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

/**
 * @brief Proxy függvény a Debug Messenger memóriájának felszabadításához.
 */
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/**
 * @brief A Vulkan hivatalos hibaüzenet-kezelője (Callback).
 * Ha a Vulkan helytelen memóriahasználatot vagy API hívást észlel, ez a függvény írja ki a konzolra.
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    std::cerr << ">>> VULKAN VALIDATION LAYER: " << pCallbackData->pMessage << std::endl << std::endl;
    return VK_FALSE;
}

// ============================================================================
// ALAPRENDSZER (INSTANCE ÉS DEVICE) FELÉPÍTÉSE
// ============================================================================

/**
 * @brief Létrehozza a Vulkan Instance-t (A program és a Vulkan API közötti fő kapcsolatot).
 * Beállítja a szükséges kiterjesztéseket az ablakkezeléshez (GLFW) és a platform-specifikus (macOS) működéshez.
 */
void VulkanContext::createInstance() {
    if (enableValidationLayers) {
        std::cout << "Validation layerek bekapcsolva!" << std::endl;
    }

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

    // Lekérdezzük a GLFW-től, hogy milyen Vulkan kiterjesztések kellenek az ablak megrajzolásához
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

// Mac kompatibilitás (Apple Silicon / MoltenVK) beállítása
#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validációs rétegek (Hibakereső) beállítása
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vulkan Instance-t!");
    }

    std::cout << "Vulkan Instance sikeresen letrehozva!" << std::endl;
}

/**
 * @brief Beköti a memóriába a hibakereső funkciókat.
 */
void VulkanContext::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult beallitani a Debug Messengert!");
    }
}

/**
 * @brief Létrehoz egy felületet (Surface), amely összeköti a Vulkan-t az operációs rendszer ablakával.
 */
void VulkanContext::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Window Surface-t!");
    }
    std::cout << "Window Surface sikeresen letrehozva!" << std::endl;
}

/**
 * @brief Megkeresi és kiválasztja a feladatra legalkalmasabb fizikai videókártyát a gépben.
 */
void VulkanContext::pickPhysicalDevice() {
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

/**
 * @brief Létrehozza a Logikai Eszközt (Logical Device), amely a kiválasztott GPU interfésze.
 * Létrehozza a grafikus és megjelenítési parancsokat fogadó sorokat (Queues).
 */
void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily: uniqueQueueFamilies) {
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

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;

    std::vector<const char *> actualDeviceExtensions = deviceExtensions;
#ifdef __APPLE__
    actualDeviceExtensions.push_back("VK_KHR_portability_subset"); // Szükséges a Mac MoltenVK hídjához
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(actualDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = actualDeviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a logikai eszközt!");
    }

    // Elmentjük a GPU parancssorainak (Queues) memóriacímét későbbi használatra
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// ============================================================================
// SWAPCHAIN ÉS KÉPKEZELÉS (Megjelenítési futószalag)
// ============================================================================

/**
 * @brief Létrehozza a Swapchain-t (Képcserélő lánc), amely a monitoron megjelenő képeket (Vásznakat) kezeli.
 * Megakadályozza a képtörést (Screen Tearing), és biztosítja a Double/Triple Buffering folyamatát.
 */
void VulkanContext::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Képek számának meghatározása (Minimális + 1 a simább működésért)
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Erre a képre fogunk rajzolni

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // Ha a rajzolás és a képernyőre küldés más fizikai részlegen (Queue) van a GPU-n
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Hatékonyabb módszer (általában ez fut)
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Swap Chain-t!");
    }

    // A lefoglalt képek elkérése a Vulkantól
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    std::cout << "Swap Chain és " << imageCount << " db vaszon sikeresen letrehozva!" << std::endl;
}

/**
 * @brief Képnézetek (Image Views) létrehozása a Swapchain nyers képeihez.
 * A Vulkan a memóriában lévő nyers képeket csak egy "Nézeten" (lencsén) keresztül tudja használni rajzolásra.
 */
void VulkanContext::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = VulkanUtils::createImageView(device, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// ============================================================================
// HARDVER ELLENŐRZŐ SEGÉDFÜGGVÉNYEK
// ============================================================================

/**
 * @brief Kitölti a Debug Messenger konfigurációs struktúráját (milyen szintű hibákat írjon ki).
 */
void VulkanContext::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

/**
 * @brief Ellenőrzi, hogy a talált GPU alkalmas-e a Vulkan motor futtatására.
 * @return bool Igaz, ha támogatja a szükséges kiterjesztéseket, a Swapchain-t és van grafikus Queue-ja.
 */
bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    std::cout << "Megtalalt GPU: " << deviceProperties.deviceName << std::endl;
    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

/**
 * @brief Ellenőrzi az adott GPU-n elérhető kiterjesztéseket (Extensions).
 */
bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

#ifdef __APPLE__
    requiredExtensions.insert("VK_KHR_portability_subset");
#endif

    for (const auto &extension: availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty(); // Igaz, ha minden szükségeset megtalált
}

/**
 * @brief Megkeresi, hogy a videókártyán melyik "család" (Queue) támogatja a grafikus számításokat
 * és a képernyőre való rajzolást (Presentation).
 */
QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily: queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
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

/**
 * @brief Lekérdezi, hogy a videókártya Swapchain-je milyen paramétereket támogat (képformátumok, vsync módok).
 */
SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

/**
 * @brief Kiválasztja a legjobb színformátumot (lehetőleg SRGB-t) az elérhetőek közül.
 */
VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat: availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

/**
 * @brief Kiválasztja a képfrissítési stratégiát (VSync). A MAILBOX mód (Triple Buffering) a preferált.
 */
VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode: availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Szabvány VSync (garantáltan támogatott)
}

/**
 * @brief Meghatározza a Swapchain felbontását az aktuális ablak méretei alapján.
 */
VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        // Levágjuk (clamp) a méretet, ha az operációs rendszer ablakmérete túllépné a GPU fizikai korlátait
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}