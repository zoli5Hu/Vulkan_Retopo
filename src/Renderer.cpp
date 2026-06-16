#include "Renderer.h"
#include <stdexcept>
#include <array>
#include <iostream>
#include <ostream>

#include "imgui_impl_vulkan.h"

/**
 * @brief Inicializálja a Renderer osztályt és felépíti a rajzoláshoz szükséges infrastruktúrát.
 * Meghívja a Render Pass, a Framebufferek, a Parancsraktár (Command Pool) és a
 * szinkronizációs objektumok létrehozásáért felelős belső függvényeket.
 * * @param vulkanContext Mutató a fő Vulkan kontextusra (eszköz, swapchain, queue-k).
 * @param depthImageView A mélységi teszthez (Z-Buffer) használt képnézet.
 * @param depthFormat A mélységi puffer formátuma (pl. VK_FORMAT_D32_SFLOAT).
 */
Renderer::Renderer(VulkanContext* vulkanContext, VkImageView depthImageView, VkFormat depthFormat) {
    this->vulkanContext = vulkanContext;
    this->depthImageView = depthImageView;

    createRenderPass(depthFormat);
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

/**
 * @brief Destruktor: Felszabadítja a Renderer által lefoglalt Vulkan erőforrásokat.
 * A törlés sorrendje kritikus, és szigorúan a Vulkan objektumok hierarchiáját követi.
 */
Renderer::~Renderer() {
    // 1. Szinkronizációs objektumok törlése
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vulkanContext->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(vulkanContext->device, inFlightFences[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
        vkDestroySemaphore(vulkanContext->device, renderFinishedSemaphores[i], nullptr);
    }

    // 2. A Command Pool törlése (ez automatikusan törli a benne lévő Command Buffereket is)
    vkDestroyCommandPool(vulkanContext->device, commandPool, nullptr);

    // 3. Framebufferek törlése
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(vulkanContext->device, framebuffer, nullptr);
    }

    // 4. A Render Pass törlése
    vkDestroyRenderPass(vulkanContext->device, renderPass, nullptr);
}

/**
 * @brief Létrehozza a Render Pass-t (Megjelenítési Tervet).
 * Ez definiálja a GPU számára, hogy milyen memóriaterületekre (Attachments) fog rajzolni
 * (szín és mélység), és mi történjen ezekkel a területekkel a rajzolás előtt (Törlés/Clear)
 * és után (Mentés/Store).
 */
void Renderer::createRenderPass(VkFormat depthFormat) {
    // 1. Szín-csatolmány (A Swapchain vászna)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vulkanContext->swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // Képkocka elején: törölje feketére
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;     // Képkocka végén: mentse el a memóriába (hogy megjelenhessen)
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Készítse fel a monitornak való átadásra

    // 2. Mélység-csatolmány (Z-Buffer)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // Képkocka elején: ürítse ki a mélységet (1.0 = legmesszebb)
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Képkocka végén: eldobható, nem kell a monitornak
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Hivatkozások a fenti csatolmányokra a Subpass számára
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Egy konkrét művelet (Subpass) a Render Pass-on belül
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // A Render Pass összeszerelése
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(vulkanContext->device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Render Pass-t!");
    }
}

/**
 * @brief Létrehozza a Framebuffer-eket (Képkeret-tartályokat).
 * Összekapcsolja a Render Pass logikai "receptjét" a Swapchain konkrét, fizikai képeivel (ImageViews)
 * és a közös mélységi pufferrel (Z-Buffer). Minden Swapchain képhez saját Framebuffer kell.
 */
void Renderer::createFramebuffers() {
    swapChainFramebuffers.resize(vulkanContext->swapChainImageViews.size());

    // Végigmegyünk a Swapchain képeken
    for (size_t i = 0; i < vulkanContext->swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            vulkanContext->swapChainImageViews[i], // Szín (egyedi minden képkockánál)
            depthImageView                         // Mélység (közös az összesnél)
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = vulkanContext->swapChainExtent.width;
        framebufferInfo.height = vulkanContext->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanContext->device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a Framebuffert!");
        }
    }
}

/**
 * @brief Létrehozza a Command Pool-t (Parancsraktárat).
 * Egy memóriatároló, ami a Command Bufferek számára foglal le memóriát az adott kártyán.
 * A poolnak ismernie kell a Graphics Queue (grafikus futószalag) indexét.
 */
void Renderer::createCommandPool() {
    // Biztonságos Queue Family keresés (hol van a grafikus részleg a kártyán)
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkanContext->physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
            break;
        }
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsFamilyIndex;
    // RESET_COMMAND_BUFFER_BIT: Lehetővé teszi, hogy minden képkockánál újraírjuk a parancsokat (dinamikus rajzolás)
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vulkanContext->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Pool-t!");
    }
}

/**
 * @brief Lefoglalja a Command Buffer-eket (Parancslistákat).
 * Annyi Command Buffer-t hozunk létre, ahány képkocka egyidejűleg "röptében" (in flight) lehet.
 */
void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Közvetlenül a queue-ba küldhető
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(vulkanContext->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult letrehozni a Command Buffereket!");
    }
}

/**
 * @brief Rögzíti a CPU által kért rajzolási parancsokat a Command Buffer-be.
 * Ez a metódus állítja össze a végleges "csomagot", amit a GPU egyben fog lefuttatni.
 * * @param commandBuffer Az aktív parancslista, amibe rögzítünk.
 * @param imageIndex A Swapchain kép indexe (a Framebuffer kiválasztásához).
 * @param pipeline A grafikus futószalag beállításai (Shaderek, Pipeline Layout).
 * @param models A kirajzolandó 3D modellek listája (High Poly, Low Poly).
 * @param descriptorSet A kamera mátrixait tartalmazó memóriaterület (UBO) hivatkozása.
 */
void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, Pipeline* pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult elkezdeni a Command Buffer rogziteset!");
    }

    // A Render Pass indítási paraméterei
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = vulkanContext->swapChainExtent;

    // Háttérszín (Sötétszürke) és Mélység (1.0) törlési értékei
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Elindítja a Render Pass-t a GPU-n
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // BIZTONSÁGI ELLENŐRZÉS MAC GPU-KHOZ: Megnézzük, van-e egyáltalán betöltött modell
    bool hasLoadedModel = false;
    for (const auto& model : models) {
        if (model.isLoaded) {
            hasLoadedModel = true;
            break;
        }
    }

    // Csak akkor kötjük be a 3D-s motorunkat (Pipeline és UBO), ha van geometria, amit kirajzolhatunk
    if (hasLoadedModel) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

        // Végigmegyünk az összes betöltött modellen, és beküldjük őket a GPU-nak
        for (const auto& model : models) {
            if (!model.isLoaded) continue;

            VkBuffer vertexBuffers[] = {model.vertexBuffer};
            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Tényleges rajzolás! (Draw Call indexek alapján)
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model.indices.size()), 1, 0, 0, 0);
        }
    }

    // IMGUI KIRAJZOLÁSA: Közvetlenül a 3D-s tér fölé rajzolja a UI-t
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    // Render Pass vége
    vkCmdEndRenderPass(commandBuffer);

    // KÖTELEZŐ LEZÁRNI A PARANCSLISTÁT!
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult befejezni a Command Buffer rogziteset!");
    }
}

/**
 * @brief Létrehozza a szinkronizációs objektumokat.
 * A Vulkan teljesen aszinkron. Szükségünk van Semaphores-okra a GPU-n belüli lépések sorrendjének
 * meghatározásához (Rajzolás várjon a Képkérésre), és Fences-ekre (Kerítések) a CPU megállításához,
 * hogy ne küldjön új parancsot, amíg a GPU be nem fejezte az előző képkockát.
 */
void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(vulkanContext->swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // SIGNALED állapotban indul, hogy a legelső frame ne várjon "örökké" az indulásra
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vulkanContext->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanContext->device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a szinkronizacios objektumokat!");
        }
    }

    for (size_t i = 0; i < vulkanContext->swapChainImages.size(); i++) {
        if (vkCreateSemaphore(vulkanContext->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Hiba: Nem sikerult letrehozni a renderFinished semaphoret!");
        }
    }
}

/**
 * @brief Kiszámol, megrajzol és megjelenít egyetlen képkockát a képernyőn.
 * Ez a fő ciklus legfontosabb magja. Kezeli a szinkronizációt, összeállítja a Command Buffer-t,
 * elküldi a videókártyának, majd a Present Queue segítségével kiküldi a monitornak.
 */
void Renderer::drawFrame(Pipeline *pipeline, const std::vector<ModelData>& models, VkDescriptorSet descriptorSet) {
    // 1. Megvárjuk, míg az előző képkocka számolása befejeződik a GPU-n (CPU blokkolása)
    vkWaitForFences(vulkanContext->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // 2. Szabad kép elkérése a Swapchain-ből (ha nincs szabad kép, ez várni fog)
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(vulkanContext->device, vulkanContext->swapChain, UINT64_MAX,
                          imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    // BIZTONSÁGI ELLENŐRZÉS MAC-RE (Ablak átméretezés vagy fagyás elkerülése)
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return; // Átugorjuk a képkockát, ha az ablak épp méreteződik
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult kepet szerezni a Swap Chain-bol!");
    }

    // 3. CSAK AKKOR zárjuk le a kerítést, ha tényleg rajzolni is fogunk!
    vkResetFences(vulkanContext->device, 1, &inFlightFences[currentFrame]);

    // Töröljük a régi parancsokat az aktuális Command Bufferből
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    // Itt történik a varázslat: rögzítjük az új rajzolási utasításokat
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, pipeline, models, descriptorSet);

    // 4. Parancsok beküldése a GPU Graphics Queue-jába
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // A rajzolással meg kell várni, amíg az "imageAvailable" szemafor zöld utat ad
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    // A rögzített Command Buffer átadása
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    // Amikor végzett a rajzolás, jelezzen a "renderFinished" szemafornak
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Végrehajtás elindítása! (Itt nyitjuk meg ismét a kerítést, ha a GPU végzett)
    if (vkQueueSubmit(vulkanContext->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Hiba: Nem sikerult bekuldeni a rajzolo parancsot!");
    }

    // 5. Megjelenítés (Present) - Az elkészült kép kiküldése az ablak felszínére
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // Várjon a rajzolás végére!

    VkSwapchainKHR swapChains[] = {vulkanContext->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(vulkanContext->presentQueue, &presentInfo);

    // A következő képkocka sorszámának (in-flight index) kiszámítása (Double/Triple Buffering)
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}