#include "RetopoApp.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanUtils.h"

//képernyő adatainak inicializálása
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

void RetopoApp::run() {
    //ablakot indító
    initWindow();
    //vulkannal való kommunikációt indítő
    initVulkan();
    //frissítő
    mainLoop();
    //memória fleszabadítás
    cleanup();
}

//ablak létrehozása
void RetopoApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "3D Retopo Tool", nullptr, nullptr);

    // --- ÚJ: Bekötjük az egeret a GLFW-be ---
    glfwSetWindowUserPointer(window, this); // Hogy a C kód lássa a C++ osztályunkat
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

void RetopoApp::initVulkan() {
    // 1. ALAPOZÁS (Ablak és Vulkan)
    vulkanContext = std::make_unique<VulkanContext>(window);

    // 2. ERŐFORRÁS MENEDZSER
    resourceManager = std::make_unique<ResourceManager>(vulkanContext->device, vulkanContext->physicalDevice);

    // 3. Z-BUFFER KÉSZÍTÉSE
    VkFormat depthFormat = VulkanUtils::findDepthFormat(vulkanContext->physicalDevice);
    resourceManager->createDepthResources(vulkanContext->swapChainExtent, depthFormat, depthImage, depthImageMemory, depthImageView);

    // 4. RAJZOLÓ (Renderer) ELINDÍTÁSA
    renderer = std::make_unique<Renderer>(vulkanContext.get(), depthImageView, depthFormat);

    // 5. GEOMETRIA BETÖLTÉSE
    loadNewModel("modells/kocka.obj");
    loadNewModel("modells/monkey.obj"); // <--- Még egy betöltése!
    // 6. KAMERA RENDSZER
    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    // 7. SHADEREK ÉS TERVRAJZ (Pipeline)
    graphicsPipeline = std::make_unique<Pipeline>(
            vulkanContext->device,
            "shaders/vert.spv",
            "shaders/frag.spv",
            renderer->getRenderPass(),
            vulkanContext->swapChainExtent,
            descriptorSetLayout
        );
}

//updater amíg be nem csukjuk
void RetopoApp::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 1. Frissítjük a kamerát/forgást az adott képkockához
        updateUniformBuffer(renderer->getCurrentFrame());

        // 2. Szólunk a Renderernek, hogy rajzolja ki az összes modellt
        renderer->drawFrame(graphicsPipeline.get(), loadedModels, descriptorSets[renderer->getCurrentFrame()]);    }
    //mindent megállít hogy nehogy töröljünk valamit amut a gpu még rajzol
    vkDeviceWaitIdle(vulkanContext->device);
}

void RetopoApp::cleanup() {
    // semaphores leállítása, mielőtt törlünk
    vkDeviceWaitIdle(vulkanContext->device);

    // 1. Végigmegyünk az összes betöltött modellen, és töröljük a memóriájukat
    for (auto& model : loadedModels) {
        vkDestroyBuffer(vulkanContext->device, model.indexBuffer, nullptr);
        vkFreeMemory(vulkanContext->device, model.indexBufferMemory, nullptr);
        vkDestroyBuffer(vulkanContext->device, model.vertexBuffer, nullptr);
        vkFreeMemory(vulkanContext->device, model.vertexBufferMemory, nullptr);
    }
    loadedModels.clear();

    // === INNEN KITÖRÖLTÜK A HIBÁS 4 SORT! ===

    //processzor és videókártya kzött kapcsolat felszabdítása
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanContext->device, uniformBuffers[i], nullptr);
        vkFreeMemory(vulkanContext->device, uniformBuffersMemory[i], nullptr);
    }
    //shader kapcsolódások törlése
    vkDestroyDescriptorPool(vulkanContext->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vulkanContext->device, descriptorSetLayout, nullptr);
    //magát a képet és a lancsét is felszabadítjuk
    vkDestroyImageView(vulkanContext->device, depthImageView, nullptr);
    vkDestroyImage(vulkanContext->device, depthImage, nullptr);
    vkFreeMemory(vulkanContext->device, depthImageMemory, nullptr);

    // 2. Modulok törlése KÖTELEZŐ SORRENDBEN (Tetőtől az alapokig)
    graphicsPipeline.reset();
    renderer.reset();        // <--- Itt törlődnek a framebufferek és a command pool automatikusan!
    resourceManager.reset();
    vulkanContext.reset();   // <--- Itt törlődik a device és az instance

    // 3. Ablak törlése
    glfwDestroyWindow(window);
    glfwTerminate();
}
//megadja a shader layout bindolást megnézi millyen típús buffer kell
void RetopoApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanContext->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Descriptor Set Layout-ot!");
    }
}

//Buffer létrehozása a folyamatosan változó kamera- és 3D mátrixoknak
void RetopoApp::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    //cpu és gpu oldalon is ofglalunk helyet hogy tudjanak külön működni
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

//descriptonseteknek előre lefoglal memória terület
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

//megadja a buffer címát a shadernek
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
    //minden képnek amit előkészítünk fogallunk helyet
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

//képváltozások kezelése memória szinten

//képváltozások kezelése memória szinten (KAMERA MATEMATIKA)
void RetopoApp::updateUniformBuffer(uint32_t currentFrame) {
    UniformBufferObject ubo{};

    // 1. MODEL: Nincs több idő-alapú pörgés! A modellünk fixen áll a tér közepén.
    ubo.model = glm::mat4(1.0f);

    // 2. VIEW: Kiszámoljuk a Gömbkoordinátákból (Yaw, Pitch, Radius), hogy hol van a kamera
    float camX = cameraTarget.x + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    float camY = cameraTarget.y + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    float camZ = cameraTarget.z + cameraRadius * sin(glm::radians(cameraPitch));

    glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);

    // Kamera beállítása: (Kamera pozíciója, Mit nézünk, Merre van a "fent")
    ubo.view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 0.0f, 1.0f));

    // 3. PROJ: A lencse torzítása
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                vulkanContext->swapChainExtent.width / (float) vulkanContext->swapChainExtent.height,
                                0.1f, 10.0f);

    ubo.proj[1][1] *= -1; // A Vulkan Y-tengely korrekciója

    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

// =========================================================
// EGÉR KEZELŐ FÜGGVÉNYEK (BLENDER KAMERA)
// =========================================================

// Statikus hidak a GLFW (C nyelv) és az osztályunk (C++) között
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

// Belső logika
// Belső logika
// Belső logika
void RetopoApp::onMouseButton(int button, int action, int mods) {
    // ÚJ: Elfogadjuk a Középső gombot (Windows egér) ÉS a Bal gombot (Mac Trackpad) is!
    if (button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Ha nyomva van a Shift, akkor Panning (eltolás) módba lépünk
            if (mods & GLFW_MOD_SHIFT) {
                isPanning = true;
            } else {
                isOrbiting = true;
            }
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY); // Megjegyezzük, hol volt az egér kattintáskor
        } else if (action == GLFW_RELEASE) {
            isOrbiting = false;
            isPanning = false; // Gomb felengedésekor leállítjuk
        }
    }
}
void RetopoApp::onCursorPos(double xpos, double ypos) {
    // Mennyit mozdult az egér/ujj az előző képkocka óta?
    float deltaX = static_cast<float>(xpos - lastMouseX);
    float deltaY = static_cast<float>(ypos - lastMouseY);

    if (isOrbiting) {
        // Sima forgás (Blender stílus)
        cameraYaw -= deltaX * 0.5f;
        cameraPitch += deltaY * 0.5f;

        // "Satu": Ne tudjon a kamera átfordulni a feje tetejére
        if (cameraPitch > 89.0f) cameraPitch = 89.0f;
        if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    }
    else if (isPanning) {
        // Nézet eltolása (Shift + Kattintás + Húzás)
        float sensitivity = 0.002f * cameraRadius;

        // Kiszámoljuk, merre van a kamera "jobbra" iránya a vízszintes forgás (Yaw) alapján
        float radiansYaw = glm::radians(cameraYaw);
        glm::vec3 cameraRight = glm::vec3(-sin(radiansYaw), cos(radiansYaw), 0.0f);

        // A "felfelé" irányt a Z tengely adja
        glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);

        // Eltoljuk a fókuszpontot az egér mozgása alapján
        cameraTarget += cameraRight * (-deltaX * sensitivity);
        cameraTarget += cameraUp * (deltaY * sensitivity);
    }

    // Frissítjük a koordinátákat a következő körhöz
    lastMouseX = xpos;
    lastMouseY = ypos;
}

void RetopoApp::onScroll(double yoffset) {
    // Zoom in / Zoom out
    cameraRadius -= static_cast<float>(yoffset) * 0.5f;

    // Biztonsági korlátok, hogy ne menjünk túl közel vagy túl távol
    if (cameraRadius < 0.5f) cameraRadius = 0.5f;
    if (cameraRadius > 20.0f) cameraRadius = 20.0f;
}

void RetopoApp::loadNewModel(const std::string& filepath) {
    ModelData newModel{};

    // Betöltjük az adatokat a fájlból
    resourceManager->loadModel(filepath, newModel.vertices, newModel.indices);
    // Létrehozzuk hozzá a Vulkan buffereket
    resourceManager->createVertexBuffer(newModel.vertexBuffer, newModel.vertexBufferMemory, newModel.vertices);
    resourceManager->createIndexBuffer(newModel.indexBuffer, newModel.indexBufferMemory, newModel.indices);

    // Hozzáadjuk a listánkhoz
    loadedModels.push_back(newModel);
}