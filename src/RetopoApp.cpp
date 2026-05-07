#include "RetopoApp.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <set> //  könyvtár a duplikációk elkerüléséhez

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
    createSwapChain(); // <---  Swap Chain létrehozása
    createImageViews(); // <--- ÚJ: Image View-k létrehozása
}

void RetopoApp::mainLoop() {
    //program ablak folyamatos futása a be nem zárásig itt fogjuk pl a frameket rajzolni
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void RetopoApp::cleanup() {
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    //  Swap Chain törlése (először ezt töröljük, mielőtt a logikai eszközt kinyírjuk)
    vkDestroySwapchainKHR(device, swapChain, nullptr);
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
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // <--- EZ AZ  SOR
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
    std::vector<const char*> actualDeviceExtensions = deviceExtensions;
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
VkSurfaceFormatKHR RetopoApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    // Ha nincs sRGB, jó lesz az első is, amit a kártya ajánl
    return availableFormats[0];
}

// 2. Megjelenítési mód (Keressük a Triple Bufferinget)
VkPresentModeKHR RetopoApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    // A V-Sync (FIFO) kötelezően támogatott mindenhol, ez a tökéletes B-terv
    return VK_PRESENT_MODE_FIFO_KHR;
}

// 3. Felbontás beállítása (GLFW ablakméret és monitor pixelek összehangolása)
VkExtent2D RetopoApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
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
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

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

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Ne forgassa el a képet (pl. telefonokon lehetne)
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ne legyen átlátszó az ablakunk háttere
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // Ha egy másik ablak kitakarja a miénket, ott ne számoljon a GPU pixeleket (optimalizáció)
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

// --- ÚJ FÜGGVÉNY: Image View-k (Nézetek) létrehozása ---
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


