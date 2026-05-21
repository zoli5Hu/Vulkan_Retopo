#include <gtest/gtest.h>
#include "../src/Pipeline.h"
#include "../src/RetopoApp.h"
#include <stdexcept>

// =================================================================
// 1. KÖRNYEZETI TESZTEK (Sanity Checks)
// =================================================================
// Ellenőrizzük, hogy a Google Test keretrendszer megfelelően fut-e.
TEST(EnvironmentTest, GTestIsWorking) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
}

// =================================================================
// 2. PIPELINE TESZTEK (Fájlkezelés és Inicializálás)
// =================================================================
// A Pipeline-nak "runtime_error" kivételt kell dobnia, ha nem létező
// shader fájlokat próbálunk meg betölteni.
TEST(PipelineTest, ThrowsErrorOnMissingShaderFile) {
    // Mivel a fájlbeolvasás a Vulkan inicializálása ELŐTT történik a Pipeline-ban,
    // nyugodtan átadhatunk "VK_NULL_HANDLE" (üres) értékeket a hardveres paramétereknek.
    VkExtent2D dummyExtent = {800, 600};

    // Az EXPECT_THROW figyeli, hogy a kódblokk dob-e megadott típusú kivételt.
    EXPECT_THROW({
        Pipeline badPipeline(VK_NULL_HANDLE,
                             "shaders/nem_letezo_vert.spv",
                             "shaders/nem_letezo_frag.spv",
                             VK_NULL_HANDLE,
                             dummyExtent,
                             VK_NULL_HANDLE); // <--- EZ HIÁNYZOTT (A 6. paraméter)
    }, std::runtime_error);
}

// =================================================================
// 3. APPLIKÁCIÓ TESZTEK
// =================================================================
// Leteszteljük, hogy az alkalmazás objektum sikeresen példányosítható-e
// anélkül, hogy memóriaszivárgást vagy azonnali összeomlást okozna.
TEST(RetopoAppTest, AppCanBeInstantiated) {
    // Csak példányosítjuk, de nem hívjuk meg a run() függvényt,
    // mert az egy végtelen while ciklust indítana (ablak).
    EXPECT_NO_THROW({
        RetopoApp app;
    });
}

// =================================================================
// 4. LOGIKAI TESZTEK (Unit Tests)
// =================================================================
TEST(QueueFamilyTest, IsCompleteLogicChecks) {
    QueueFamilyIndices indices;

    // 1. Üresen nem lehet teljes
    EXPECT_FALSE(indices.isComplete());

    // 2. Csak grafikus sorral még nem teljes
    indices.graphicsFamily = 0;
    EXPECT_FALSE(indices.isComplete());

    // 3. Ha mindkettő megvan, akkor teljesnek kell lennie
    indices.presentFamily = 1;
    EXPECT_TRUE(indices.isComplete());
}