#include "RetopoApp.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <glm/gtc/matrix_transform.hpp>

#include "nfd.hpp"
#include "VulkanUtils.h"

// Képernyő alapértelmezett felbontása
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

/**
 * @brief Az alkalmazás fő életciklusa.
 * Sorrendben inicializálja az ablakot, a Vulkant, futtatja a fő ciklust, majd a végén takarít.
 */
void RetopoApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

/**
 * @brief Létrehozza a platformfüggetlen ablakot a GLFW segítségével.
 * Inicializálja az egér eseménykezelőit (Callback-ek), és összeköti a C-típusú GLFW
 * ablakot az objektumorientált C++ osztállyal (UserPointer).
 */
void RetopoApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Nem kérünk OpenGL kontextust, mert Vulkant használunk
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Egyelőre fix méretű ablak
    window = glfwCreateWindow(WIDTH, HEIGHT, "3D Retopo Tool", nullptr, nullptr);

    // Összekötjük az ablakot ezzel az osztálypéldánnyal (this), hogy a statikus függvények elérjék a tagváltozókat
    glfwSetWindowUserPointer(window, this);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

/**
 * @brief Felépíti a teljes Vulkan grafikus motort és a külső könyvtárakat (ImGui, NFD).
 */
void RetopoApp::initVulkan() {
    // 1. Alaprendszer (Instance, Device, Swapchain)
    vulkanContext = std::make_unique<VulkanContext>(window);

    // 2. Memóriakezelő (Modellek és Bufferek)
    resourceManager = std::make_unique<ResourceManager>(vulkanContext->device, vulkanContext->physicalDevice);

    // 3. Mélységi teszt (Z-Buffer) a 3D takarásokhoz
    VkFormat depthFormat = VulkanUtils::findDepthFormat(vulkanContext->physicalDevice);
    resourceManager->createDepthResources(vulkanContext->swapChainExtent, depthFormat, depthImage, depthImageMemory, depthImageView);

    // 4. Rajzoló mester (Renderer) elindítása
    renderer = std::make_unique<Renderer>(vulkanContext.get(), depthImageView, depthFormat);

    // 5. Két üres "fiók" (slot) lefoglalása a High Poly és Low Poly modelleknek
    loadedModels.resize(2);

    // 6. Kamera és transzformációs adatok (UBO) előkészítése
    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // 7. A grafikus futószalag (Shaderek és beállítások) összerakása
    graphicsPipeline = std::make_unique<Pipeline>(
            vulkanContext->device,
            "shaders/vert.spv",
            "shaders/frag.spv",
            renderer->getRenderPass(),
            vulkanContext->swapChainExtent,
            descriptorSetLayout
        );

    // 8. Felhasználói felület (UI) rendszereinek indítása
    initImGui();
    NFD::Init(); // Natív fájltallózó inicializálása
}

/**
 * @brief A program fő futási ciklusa (Game Loop).
 * * Frissíti az ablak eseményeit, kezeli a UI (ImGui) rajzolását és logikáját,
 * felügyeli a fájlműveleteket, majd kiküldi a rajzolási parancsot a Vulkan Renderernek.
 */
void RetopoApp::mainLoop() {
    // Állapotjelzők a fájlkezelő aszinkron megnyitásához
    bool openHighPolyDialog = false;
    bool openLowPolyDialog = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- 1. IMGUI ÚJ KÉPKOCKA INDÍTÁSA ---
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- 2. FELHASZNÁLÓI FELÜLET (UI) DEFINIÁLÁSA ---
        ImGui::Begin("Retopo Eszkozok");

        // High Poly szekció
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "HIGH POLY (Eredeti Modell)");
        ImGui::Text("Fajl: %s", highPolyName.c_str());
        if (ImGui::Button("Fajl Kivalasztasa...##hp")) {
            openHighPolyDialog = true; // Csak felkapcsoljuk a jelet, itt még nem blokkoljuk a programot!
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Low Poly szekció
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "LOW POLY (Szerkesztett)");
        ImGui::Text("Fajl: %s", lowPolyName.c_str());
        if (ImGui::Button("Fajl Kivalasztasa...##lp")) {
            openLowPolyDialog = true;
        }

        ImGui::End();

        // --- 3. IMGUI ADATOK VÉGLEGESÍTÉSE ---
        ImGui::Render();

        // --- 4. FÁJLTALLÓZÓK KEZELÉSE ---
        // Fontos: Ezt a UI renderelés után kell meghívni, hogy az ablak megnyitása ne fagyassza ki a grafikus motort.
        if (openHighPolyDialog) {
            NFD::UniquePath outPath;
            nfdfilteritem_t filterItem[1] = { { "OBJ Modellek", "obj" } };
            if (NFD::OpenDialog(outPath, filterItem, 1) == NFD_OKAY) {
                loadModelIntoSlot(0, outPath.get(), highPolyName);
            }
            openHighPolyDialog = false;
            ImGui::GetIO().MouseDown[0] = false; // Biztosíték az ImGui kattintás-beragadás ellen
        }

        if (openLowPolyDialog) {
            NFD::UniquePath outPath;
            nfdfilteritem_t filterItem[1] = { { "OBJ Modellek", "obj" } };
            if (NFD::OpenDialog(outPath, filterItem, 1) == NFD_OKAY) {
                loadModelIntoSlot(1, outPath.get(), lowPolyName);
            }
            openLowPolyDialog = false;
            ImGui::GetIO().MouseDown[0] = false;
        }

        // --- 5. KAMERA ÉS VULKAN RAJZOLÁS ---
        updateUniformBuffer(renderer->getCurrentFrame());
        renderer->drawFrame(graphicsPipeline.get(), loadedModels, descriptorSets[renderer->getCurrentFrame()]);
    }

    // Megvárjuk, amíg a GPU befejezi a munkát, mielőtt kilépünk a ciklusból
    vkDeviceWaitIdle(vulkanContext->device);
}

/**
 * @brief Memória felszabadítás szigorú fordított sorrendben.
 * Ha nem várunk a GPU leállására, vagy rossz sorrendben törlünk (pl. eldobom a memóriát, de még
 * rajta van egy textúra), a Vulkan azonnal Segmentation Fault hibát dob.
 */
void RetopoApp::cleanup() {
    vkDeviceWaitIdle(vulkanContext->device);

    // 1. ImGui leállítása
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(vulkanContext->device, imguiPool, nullptr);

    // 2. Betöltött modellek és GPU memóriáik felszabadítása
    for (auto& model : loadedModels) {
        if (model.isLoaded) {
            vkDestroyBuffer(vulkanContext->device, model.indexBuffer, nullptr);
            vkFreeMemory(vulkanContext->device, model.indexBufferMemory, nullptr);
            vkDestroyBuffer(vulkanContext->device, model.vertexBuffer, nullptr);
            vkFreeMemory(vulkanContext->device, model.vertexBufferMemory, nullptr);
        }
    }
    loadedModels.clear();

    // 3. Kamera adatok (UBO) törlése
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanContext->device, uniformBuffers[i], nullptr);
        vkFreeMemory(vulkanContext->device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(vulkanContext->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, descriptorSetLayout, nullptr);

    // 4. Z-Buffer törlése
    vkDestroyImageView(vulkanContext->device, depthImageView, nullptr);
    vkDestroyImage(vulkanContext->device, depthImage, nullptr);
    vkFreeMemory(vulkanContext->device, depthImageMemory, nullptr);

    // 5. Fő modulok törlése (Az okos mutatók /std::unique_ptr/ meghívják a destruktoraikat)
    graphicsPipeline.reset();
    renderer.reset();
    resourceManager.reset();
    vulkanContext.reset();

    // 6. Rendszerszintű ablakkezelők leállítása
    NFD::Quit();
    glfwDestroyWindow(window);
    glfwTerminate();
}

/**
 * @brief Létrehozza a Descriptor Set Layout-ot.
 * Ez határozza meg a Shader számára, hogy milyen memóriablokkokra (pl. Uniform Bufferek) számíthat.
 */
void RetopoApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // A Vertex Shaderben használjuk (kamera transzformáció)

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Set Layout-ot!");
    }
}

/**
 * @brief Lefoglalja a Uniform Buffer-eket (UBO), amelyek a kamera változó adatait továbbítják a GPU-nak.
 * Minden aktív képkockához (MAX_FRAMES_IN_FLIGHT) külön puffer tartozik a párhuzamosítás érdekében.
 */
void RetopoApp::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(vulkanContext->device, &bufferInfo, nullptr, &uniformBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Uniform Buffert!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vulkanContext->device, uniformBuffers[i], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(vulkanContext->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(vulkanContext->device, &allocInfo, nullptr, &uniformBuffersMemory[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult memoriat foglalni a Uniform Buffernek!");
        }

        vkBindBufferMemory(vulkanContext->device, uniformBuffers[i], uniformBuffersMemory[i], 0);
        vkMapMemory(vulkanContext->device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

/**
 * @brief Létrehozza a Descriptor Pool-t, amelyből a tényleges memóriakötéseket le lehet foglalni.
 */
void RetopoApp::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(vulkanContext->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Pool-t!");
    }
}

/**
 * @brief Beköti a Uniform Buffereket a Shaderek számára (Descriptor Sets generálás).
 */
void RetopoApp::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(vulkanContext->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
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

        vkUpdateDescriptorSets(vulkanContext->device, 1, &descriptorWrite, 0, nullptr);
    }
}

/**
 * @brief Kiszámítja és feltölti a kameramátrixokat a GPU memóriába (minden képkockán lefut).
 * Gömbkoordinátákat (Yaw, Pitch, Radius) alakít át Descartes-koordinátákra (X, Y, Z),
 * ezáltal egy orbitális, fókuszpont körüli (Blender-szerű) kameramozgást tesz lehetővé.
 */
void RetopoApp::updateUniformBuffer(uint32_t currentFrame) {
    UniformBufferObject ubo{};

    // 1. Model Mátrix: Fixen tartja a modellt az origóban
    ubo.model = glm::mat4(1.0f);

    // 2. View Mátrix: Kamera pozíciójának számítása gömbkoordinátákból
    float camX = cameraTarget.x + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    float camY = cameraTarget.y + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    float camZ = cameraTarget.z + cameraRadius * sin(glm::radians(cameraPitch));

    glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);
    ubo.view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 0.0f, 1.0f));

    // 3. Projection Mátrix: Perspektivikus torzítás beállítása (45 fokos látószög)
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                vulkanContext->swapChainExtent.width / (float) vulkanContext->swapChainExtent.height,
                                0.1f, 10.0f);

    // A Vulkan koordinátarendszere fordított az OpenGL-hez képest, így az Y tengelyt invertáljuk
    ubo.proj[1][1] *= -1;

    // Az adatok áttolása a CPU RAM-ból a leképezett GPU VRAM-ba
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

// =========================================================
// EGÉR KEZELŐ FÜGGVÉNYEK (BLENDER KAMERA)
// =========================================================

// Statikus hidak a C-alapú GLFW hívások és a C++ osztálypéldány között
void RetopoApp::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto app = reinterpret_cast<RetopoApp*>(glfwGetWindowUserPointer(window));
    app->onMouseButton(button, action, mods);
}
void RetopoApp::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<RetopoApp*>(glfwGetWindowUserPointer(window));
    app->onCursorPos(xpos, ypos);
}
void RetopoApp::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto app = reinterpret_cast<RetopoApp*>(glfwGetWindowUserPointer(window));
    app->onScroll(yoffset);
}

/**
 * @brief Kezeli az egérgomb lenyomását. Dönt az Orbit (keringés) vagy a Pan (eltolás) módok között.
 */
void RetopoApp::onMouseButton(int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    // Ha az egér a UI-on (menün) van, a kamera interakció blokkolása
    if (io.WantCaptureMouse) return;

    // Támogatja a középső (PC) és a bal (Mac Trackpad) gombot is
    if (button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            if (mods & GLFW_MOD_SHIFT) {
                isPanning = true;
            } else {
                isOrbiting = true;
            }
            // Kiinduló pozíció rögzítése
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else if (action == GLFW_RELEASE) {
            isOrbiting = false;
            isPanning = false;
        }
    }
}

/**
 * @brief Kezeli az egér mozgását, frissíti a kameraszögeket és a pozíciót.
 */
void RetopoApp::onCursorPos(double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    float deltaX = static_cast<float>(xpos - lastMouseX);
    float deltaY = static_cast<float>(ypos - lastMouseY);

    if (isOrbiting) {
        cameraYaw -= deltaX * 0.5f;
        cameraPitch += deltaY * 0.5f;

        // Gimbal Lock megelőzése szoftveres "satuval"
        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    }
    else if (isPanning) {
        float sensitivity = 0.002f * cameraRadius;

        // Vízszintes és függőleges kameravektorok kiszámítása az eltoláshoz
        float radiansYaw = glm::radians(cameraYaw);
        glm::vec3 cameraRight = glm::vec3(-sin(radiansYaw), cos(radiansYaw), 0.0f);
        glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);

        cameraTarget += cameraRight * (-deltaX * sensitivity);
        cameraTarget += cameraUp * (deltaY * sensitivity);
    }

    lastMouseX = xpos;
    lastMouseY = ypos;
}

/**
 * @brief Egérgörgő kezelése a kamera fókuszpontjához való távolság (Radius) állításához (Zoom).
 */
void RetopoApp::onScroll(double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    cameraRadius -= static_cast<float>(yoffset) * 0.5f;

    if (cameraRadius < 0.5f) cameraRadius = 0.5f;
    if (cameraRadius > 20.0f) cameraRadius = 20.0f;
}

/**
 * @brief Segédfüggvény egy egyszerű modell betöltésére (főleg inicializáláskor hasznos).
 */
void RetopoApp::loadNewModel(const std::string& filepath) {
    ModelData newModel{};
    resourceManager->loadModel(filepath, newModel.vertices, newModel.indices);
    resourceManager->createVertexBuffer(newModel.vertexBuffer, newModel.vertexBufferMemory, newModel.vertices);
    resourceManager->createIndexBuffer(newModel.indexBuffer, newModel.indexBufferMemory, newModel.indices);
    loadedModels.push_back(newModel);
}

/**
 * @brief Integrálja a "Dear ImGui" azonnali módú (Immediate Mode) grafikus felületet a Vulkanba.
 * Létrehoz egy dedikált Descriptor Pool-t, és összeköti az ImGui belső állapotgépét a GLFW ablakkezelővel.
 */
void RetopoApp::initImGui() {
    // 1. Univerzális ImGui Pool (Minden memóriatípushoz elegendő hely lefoglalása)
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(vulkanContext->device, &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni az ImGui Descriptor Pool-t!");
    }

    // 2. Grafikus sor (Queue) megkeresése
    uint32_t graphicsFamily = 0;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i; break;
        }
    }

    // 3. Rendszer kontextus indítása
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 4. ImGui betöltési beállításai
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanContext->instance;
    init_info.PhysicalDevice = vulkanContext->physicalDevice;
    init_info.Device = vulkanContext->device;
    init_info.DescriptorPool = imguiPool;
    init_info.QueueFamily = graphicsFamily;
    init_info.Queue = vulkanContext->graphicsQueue;
    init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.PipelineInfoMain.RenderPass = renderer->getRenderPass();
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

/**
 * @brief Aszinkron módon, futásidőben cserél ki egy modellt a memóriában.
 * Szigorúan megvárja, amíg a GPU befejezi a munkát, majd felszabadítja az előző objektum
 * memóriáját, mielőtt beolvasná az újat, ezáltal megelőzve a memóriaszivárgást és a fagyást.
 * * @param slot A modell helye a tárolóban (0 = High Poly, 1 = Low Poly).
 * @param filepath A betöltendő új OBJ fájl elérési útja.
 * @param nameTracker Csatolt string változó, amely frissíti a menüben kiírt fájlnevet.
 */
void RetopoApp::loadModelIntoSlot(int slot, const std::string& filepath, std::string& nameTracker) {

    // 1. Biztonsági leállítás: Nem nyúlhatunk a memóriához, amíg a GPU rajzol!
    vkDeviceWaitIdle(vulkanContext->device);

    // 2. Ha az adott slotban már volt modell, előbb töröljük a memóriáját
    if (loadedModels[slot].isLoaded) {
        vkDestroyBuffer(vulkanContext->device, loadedModels[slot].indexBuffer, nullptr);
        vkFreeMemory(vulkanContext->device, loadedModels[slot].indexBufferMemory, nullptr);
        vkDestroyBuffer(vulkanContext->device, loadedModels[slot].vertexBuffer, nullptr);
        vkFreeMemory(vulkanContext->device, loadedModels[slot].vertexBufferMemory, nullptr);

        loadedModels[slot].vertices.clear();
        loadedModels[slot].indices.clear();
        loadedModels[slot].isLoaded = false;
    }

    // 3. Az új modell beolvasása a megtisztított helyre
    try {
        resourceManager->loadModel(filepath, loadedModels[slot].vertices, loadedModels[slot].indices);
        resourceManager->createVertexBuffer(loadedModels[slot].vertexBuffer, loadedModels[slot].vertexBufferMemory, loadedModels[slot].vertices);
        resourceManager->createIndexBuffer(loadedModels[slot].indexBuffer, loadedModels[slot].indexBufferMemory, loadedModels[slot].indices);

        loadedModels[slot].isLoaded = true;

        // Fájlnév formázása (Levágjuk a mappákat az olvashatóság kedvéért)
        size_t pos = filepath.find_last_of("/\\");
        nameTracker = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;

    } catch (const std::exception& e) {
        std::cerr << "Hiba a modell betoltesekor: " << e.what() << std::endl;
        nameTracker = "Hibas fajl!";
    }
}