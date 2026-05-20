#include "RetopoApp.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <set> //  könyvtár a duplikációk elkerüléséhez
#include <limits>    // <--- ÚJ: Ez kell a numeric_limits-hez
#include <algorithm> // <--- ÚJ: Ez kell a clamp-hez
#include <glm/gtc/matrix_transform.hpp> // <--- ÚJ: Mátrix forgatások
#include <chrono>                       // <--- ÚJ: Időmérés a folyamatos forgáshoz

#include "Vertex.h"
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

// A kártyától elvárt kiterjesztések listája (A Swap Chain-hez)
const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// --- : Proxy függvények a Debug Messenger betöltéséhez ---
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

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// --- : Maga a Callback függvény, ami kiírja a hibákat ---
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    std::cerr << ">>> VULKAN VALIDATION LAYER: " << pCallbackData->pMessage << std::endl << std::endl;
    return VK_FALSE;
}

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
    setupDebugMessenger(); // <---
    createSurface(); // <-- Felszín létrehozása a kártya választás ELŐTT
    pickPhysicalDevice();
    createLogicalDevice(); // Létrehozzuk a logikai eszközt
    createSwapChain(); // <---  Swap Chain létrehozása
    createImageViews(); // <--- : Image View-k létrehozása
    createRenderPass();    // --- : Render Pass létrehozása a Pipeline ELŐTT! ---
    createFramebuffers(); //a swapchanből kiválasztjuk melyik kép tarája legyen
    // --- ÚJ: Parancsraktár és Parancslista létrehozása ---
    createCommandPool();
    createCommandBuffer();
createSyncObjects();
    createVertexBuffer();

    // --- ÚJ: Kamera memóriafoglalása a Pipeline ELŐTT ---
    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // Most már átadhatjuk a descriptorSetLayout-ot a hiba elkerüléséhez!
    graphicsPipeline = std::make_unique<Pipeline>(
            device,
            "shaders/vert.spv",
            "shaders/frag.spv",
            renderPass,
            swapChainExtent,
            descriptorSetLayout // <--- JAVÍTVA!
        );


    }

void RetopoApp::mainLoop() {
    //program ablak folyamatos futása a be nem zárásig itt fogjuk pl a frameket rajzolni
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame(); // <--- folton rajzolja a frameket
    }
    // Megvárjuk, amíg a GPU végez, mielőtt bezárjuk a programot
    vkDeviceWaitIdle(device);
}

void RetopoApp::cleanup() {
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    // --- ÚJ: Kamera memóriájának törlése ---
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    // 2. Szinkronizációs lámpák (Semaphores, Fences) törlése
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    }

    // 3. Parancsraktár (Command Pool) törlése
    vkDestroyCommandPool(device, commandPool, nullptr);

    // 4. Vászon tartályok (Framebufferek) törlése
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // 5. Tervrajz (Pipeline) és Render Pass törlése
    graphicsPipeline.reset();
    vkDestroyRenderPass(device, renderPass, nullptr);

    // 6. Lencsék (ImageViews) és Csere-lánc (Swap Chain) törlése
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain, nullptr);

    // 7. A hardveres kapcsolat (Logical Device) bontása
    vkDestroyDevice(device, nullptr);

    // 8. Debugger kikapcsolása
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    // 9. Ablak felszín és Vulkan Instance törlése
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    // 10. Ablak bezárása
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
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    //megfelelő bit bekapcsolása bitmask
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // : Debug kiterjesztés hozzáadása
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // --- 2. VALIDATION LAYEREK ÉS DEBUGGER BEÁLLÍTÁSA ---
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
        // Hibák elkapása a létrehozás/törlés során
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // --- 3. VULKAN INSTANCE LÉTREHOZÁSA ---
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vulkan Instance-t!");
    }

    std::cout << "Vulkan Instance sikeresen letrehozva!" << std::endl;
}

// ---  FÜGGVÉNY: Felszín létrehozása ---
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

    //Ellenőrizzük, hogy a kártya támogatja-e a Swap Chain-t
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    //  Ellenőrizzük, hogy van-e közös formátum a monitorral (csak akkor, ha a Swap Chain kiterjesztés létezik!)
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        // Akkor megfelelő, ha legalább 1 formátumot ÉS 1 megjelenítési módot találunk
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    std::cout << "Megtalalt GPU: " << deviceProperties.deviceName << std::endl;

    // Csak akkor jó a kártya, ha mindhárom feltétel teljesül
    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

//  FÜGGVÉNY: Támogatja a kártya a kötelező kiterjesztéseket?
bool RetopoApp::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    // Egy halmazba (set) tesszük a kötelező kiterjesztéseket
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    // MAC SPECIFIKUS: Apple gépen a portability subset-et is hozzá kell adnunk a kötelező listához
#ifdef __APPLE__
    requiredExtensions.insert("VK_KHR_portability_subset");
#endif

    // Végigmegyünk a kártya által támogatottakon, és amit megtalálunk, kihúzzuk a listánkból
    for (const auto &extension: availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // Ha a lista kiürült, az azt jelenti, hogy minden kötelező kiterjesztés megvan!
    return requiredExtensions.empty();
}

//  FÜGGVÉNY: Lekérdezi az ablak/monitor tulajdonságait a Swap Chain-hez
SwapChainSupportDetails RetopoApp::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    // 1. Képességek (Capabilities) lekérdezése
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // 2. Támogatott színformátumok lekérdezése
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        //kétlépcsős azonosítás mivel nemtujuk hány formátum van ezért itt ofglalunk helyet mert már megszámoltuk
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // 3. Megjelenítési módok (Present Modes) lekérdezése
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
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
        // 2.  Tudja kezelni az ablakunkat (Surface)?
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

    // Itt átadjuk az összes szükséges Queue-t (ha 1 db, ha 2 db)
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;

    // MAC SPECIFIKUS: Ha Apple gépen vagyunk, kötelező a portability subset a logikai eszközhöz is
    std::vector<const char *> actualDeviceExtensions = deviceExtensions;
#ifdef __APPLE__
    actualDeviceExtensions.push_back("VK_KHR_portability_subset");
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

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue); //  Kép megjelenítő futószalag
}

// --- SWAP CHAIN SEGÉDFÜGGVÉNYEK ---

// 1. Színformátum kiválasztása (Keressük a szabványos sRGB-t)
VkSurfaceFormatKHR RetopoApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat: availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    // Ha nincs sRGB, jó lesz az első is, amit a kártya ajánl
    return availableFormats[0];
}

// 2. Megjelenítési mód (Keressük a Triple Bufferinget)
VkPresentModeKHR RetopoApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode: availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    // A V-Sync (FIFO) kötelezően támogatott mindenhol, ez a tökéletes B-terv
    return VK_PRESENT_MODE_FIFO_KHR;
}

// 3. Felbontás beállítása (GLFW ablakméret és monitor pixelek összehangolása)
VkExtent2D RetopoApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        // Biztosítjuk, hogy az ablak mérete ne lógjon ki a monitor által támogatott minimum és maximum közül
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// --- FŐ SWAP CHAIN LÉTREHOZÓ FÜGGVÉNY ---
void RetopoApp::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Képkockák (vásznak) száma a láncban. A minimum + 1-et kérjük, hogy a GPU ne várakozzon ránk.
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // Ellenőrizzük, hogy nem léptük-e túl a kártya által támogatott maximumot (a 0 azt jelenti, nincs felső határ)
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // A Swap Chain megrendelőlapja
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // Mindig 1, kivéve ha VR (sztereoszkopikus) 3D-t fejlesztenénk
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Erre a képre egyenesen színeket fogunk rajzolni

    // Hogyan kezeljék a képeket a különböző Queue-k (Grafikus és Megjelenítő)?
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        // Ha két külön chip felel a rajzolásért és a megjelenítésért (ritka)
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // A kártyák 99%-ánál ugyanaz a futószalag csinálja mindkettőt, ez sokkal gyorsabb
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Opcionális
        createInfo.pQueueFamilyIndices = nullptr; // Opcionális
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    // Ne forgassa el a képet (pl. telefonokon lehetne)
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ne legyen átlátszó az ablakunk háttere
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    // Ha egy másik ablak kitakarja a miénket, ott ne számoljon a GPU pixeleket (optimalizáció)
    createInfo.oldSwapchain = VK_NULL_HANDLE; // Később (pl. ablak átméretezésnél) ide jön a régi swap chain

    // Végre hozzuk létre!
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Swap Chain-t!");
    }

    // Kérjük le a legyártott vásznakat (képeket) és mentsük el őket a vektorunkba
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    // Mentsük el a formátumot és a méretet is a későbbi használathoz
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    std::cout << "Swap Chain és " << imageCount << " db vaszon sikeresen letrehozva!" << std::endl;
}

// ---  FÜGGVÉNY: Image View-k (Nézetek) létrehozása ---
//ez felel azért hogy a swapchan képeihez hozzáférjünk
void RetopoApp::createImageViews() {
    // Pontosan annyi nézet kell, ahány képünk van a láncban
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];

        // 1. Milyen típusú a kép? (1D, 2D, 3D, esetleg kocka-textúra)
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        // 2. Milyen a színformátuma? (A Swap Chain-től örököljük)
        createInfo.format = swapChainImageFormat;

        // 3. A színcsatornák "keverése" (Swizzle)
        // Az IDENTITY azt jelenti, hogy nem keverjük fel a csatornákat, a Piros marad Piros, stb.
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 4. Melyik részét akarjuk látni a képnek?
        // Mi most egy szimpla szín-képet akarunk (COLOR_BIT), mipmapok (kicsinyített másolatok) és extra rétegek nélkül.
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni az Image View-t a kephez!");
        }
    }

    std::cout << swapChainImageViews.size() << " db Image View sikeresen letrehozva!" << std::endl;
}

// ---  FÜGGVÉNYEK: Debug Messenger beállítása ---
void RetopoApp::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // Miket írjon ki? (WARNING és ERROR a legfontosabb, a VERBOSE mindent IS kiír, most azt kikapcsoljuk, hogy ne spam-eljen)
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void RetopoApp::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult beallitani a Debug Messengert!");
    }
}

// ---  FÜGGVÉNY: Render Pass (Rajzolási Fázis) létrehozása ---
void RetopoApp::createRenderPass() {
    // 1. A szín-csatolmány (A vásznunk, amire rajzolunk)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat; // Ugyanaz a formátum, mint a Swap Chain-é
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Nincs élsimítás (Multisampling) egyelőre

    // Mit csináljon a vászonnal rajzolás ELŐTT és UTÁN?
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Törölje tisztára (feketére) a képet kezdéskor!
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Mentse el az eredményt, hogy lássuk a képernyőn!

    // A Stencil (maszkolás) adatokat most nem használjuk
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Milyen állapotban van a kép kezdéskor, és mivé váljon a végén?
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Nem érdekel, mi volt rajta eddig (úgyis töröljük)
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // A végén legyen kész arra, hogy kimenjen a monitorra!

    // 2. Hivatkozás a csatolmányra (A subpass-hoz)
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // A 0. indexű csatolmányt használjuk (colorAttachment)
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // A Vulkan optimalizálja színrajzoláshoz

    // 3. Al-fázis (Subpass)
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Ez egy grafikus (nem pedig compute/számítási) fázis
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 4. Maga a Render Pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Render Pass-t!");
    }

    std::cout << "Render Pass sikeresen letrehozva!" << std::endl;
}

void RetopoApp::createFramebuffers() {
    // Pontosan annyi tartály kell, ahány képünk van
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        // Milyen nézeteket (lencséket) akarunk használni ehhez a tartályhoz?
        // Jelenleg csak egyet: a szín-csatolmányt (color attachment)
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass; // Melyik tervrajzhoz (Render Pass) tartozik?
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments; // Mi a fizikai kép, amire rajzolunk?
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Framebuffert!");
        }
    }

    std::cout << swapChainFramebuffers.size() << " db Framebuffer sikeresen letrehozva!" << std::endl;
}

// --- ÚJ FÜGGVÉNY: Parancsraktár létrehozása ---
void RetopoApp::createCommandPool() {
    // A parancsokat egy specifikus futószalagra kell küldeni (Graphics Queue)
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    // Ez a flag engedi, hogy minden képkockánál "újrahasznosítsuk" és újraírjuk a teherautó tartalmát
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Pool-t!");
    }

    std::cout << "Command Pool sikeresen letrehozva!" << std::endl;
}

// --- ÚJ FÜGGVÉNY: Parancslista (Teherautó) lefoglalása a raktárból ---
void RetopoApp::createCommandBuffer() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size(); // <--- 2-t kérünk!

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Buffereket!");
    }
}
// --- ÚJ FÜGGVÉNY: Parancsok felírása a teherautóra (Recording) ---
void RetopoApp::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult elkezdeni a Command Buffer rogziteset!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {{{0.01f, 0.01f, 0.01f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipeline());

    //  Rákötjük a Vertex Buffert a rajzolás előtt!
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // ---> ÚJ: Rákötjük a kamerát (Descriptor Set) a Pipeline-ra! <---
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Itt a 3-as szám helyett is a C++ tömb méretét kell megadni!
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult befejezni a Command Buffer rogziteset!");
    }
}
void RetopoApp::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    // ÚJ: A befejezést jelző lámpából annyi kell, ahány vászon van (3)
    renderFinishedSemaphores.resize(swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Ebből a kettőből elég a CPU-GPU szinkronizáláshoz a 2 db
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a szinkronizacios objektumokat!");
            }
    }

    // Ebből viszont vásznanként kell egy
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a renderFinished semaphoret!");
        }
    }
}
void RetopoApp::drawFrame() {
    // 1. Megvárjuk az AKTUÁLIS kerítést
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // 2. Kép kérése az AKTUÁLIS lámpával
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // --- ÚJ: Minden képkockánál kiszámoljuk az új forgást! ---
    updateUniformBuffer(currentFrame);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // 4. Beküldés a GPU-nak
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult bekuldeni a rajzolo parancsot!");
    }

    // 5. Megjelenítés a monitoron
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    // LÉPTETÉS: Ha 0 volt, 1 lesz. Ha 1 volt, 0 lesz.
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// Segédfüggvény: Megkeresi a videókártyán a megfelelő típusú memóriablokkot
uint32_t RetopoApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Hiba: Nem talalhato megfelelo tipusu memoria a GPU-n!");
}

// Fő függvény: Létrehozza a tárolót és átmásolja a pontokat
void RetopoApp::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // 1. Létrehozzuk magát a "Tároló" objektumot
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // Ez egy Vertex Buffer lesz
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Vertex Buffert!");
    }

    // 2. Megkérdezzük a Vulkant, mennyi és milyen memóriára van szüksége ennek a tárolónak
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

    // 3. Lefoglaljuk a fizikai memóriát (VRAM)
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Olyan memóriát kérünk, amit a C++ (CPU) is lát, és azonnal szinkronizálódik
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Vertex Buffernek!");
    }

    // 4. Összekötjük a tárolót a lefoglalt memóriával
    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // 5. AZ ADATOK ÁTMÁSOLÁSA (C++ -> GPU)
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data); // Kinyitjuk a memóriát
    memcpy(data, vertices.data(), (size_t) bufferSize);               // Bemásoljuk a C++ tömböt
    vkUnmapMemory(device, vertexBufferMemory);                        // Bezárjuk a memóriát
}

// --- ÚJ FÜGGVÉNYEK A 3D KAMERÁHOZ ÉS MÁTRIXOKHOZ ---

void RetopoApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Csak a Vertex shader férhet hozzá

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Set Layout-ot!");
    }
}

void RetopoApp::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; // Ez UNIFORM buffer lesz!
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Uniform Buffert!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, uniformBuffers[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &uniformBuffersMemory[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Uniform Buffernek!");
        }

        vkBindBufferMemory(device, uniformBuffers[i], uniformBuffersMemory[i], 0);

        // A memóriát FOLYAMATOSAN nyitva hagyjuk, mert minden másodpercben 60-szor írunk bele
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void RetopoApp::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Pool-t!");
    }
}

void RetopoApp::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult lefoglalni a Descriptor Set-eket!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void RetopoApp::updateUniformBuffer(uint32_t currentFrame) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    // 1. FORGÁS: Forgassuk folyamatosan a Z tengely körül!
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // 2. KAMERA: (2, 2, 2) pontból nézünk be a (0, 0, 0) origóba. Felfelé irány a Z tengely.
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // 3. PERSPEKTÍVA: 45 fokos látószög
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);

    // Vulkan Y tengelye lefelé mutat, de a GLM OpenGL-hez készült, ahol felfelé. Meg kell fordítani!
    ubo.proj[1][1] *= -1;

    // Végül bemásoljuk a friss mátrixokat a memóriába:
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}