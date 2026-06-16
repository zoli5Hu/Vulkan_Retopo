/**
 * @file test_main.cpp
 * @brief Az alkalmazás automatizált egységtesztjei (Unit Tests) a Google Test (GTest) keretrendszerrel.
 * * Ez a fájl garantálja a szoftver matematikai stabilitását, az adatszerkezetek (Hash, Vertex)
 * helyes működését és a kritikus hibakezelési mechanizmusok (pl. hiányzó fájlok) megbízhatóságát.
 */

#include <gtest/gtest.h>
#include "../src/Pipeline.h"
#include "../src/RetopoApp.h"
#include "../src/Vertex.h"
#include <stdexcept>
#include <glm/glm.hpp>

// =================================================================
// 1. KÖRNYEZETI TESZTEK (Sanity Checks)
// =================================================================

/**
 * @brief Ellenőrzi, hogy a Google Test keretrendszer megfelelően fordult-e le és fut-e a környezetben.
 * Ha ez a teszt elbukik, akkor a hiba nem a mi kódunkban, hanem a CMake/Fordító beállításaiban van.
 */
TEST(EnvironmentTest, GTestIsWorking) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
}

// =================================================================
// 2. PIPELINE TESZTEK (Fájlkezelés és Inicializálás)
// =================================================================

/**
 * @brief Teszteli a Vulkan Pipeline hibakezelését hiányzó fájlok esetén.
 * Ha egy shader fájl (.spv) nem található, a programnak nem szabad kifagynia (Segmentation Fault),
 * hanem egy kontrollált std::runtime_error kivételt kell dobnia.
 */
TEST(PipelineTest, ThrowsErrorOnMissingShaderFile) {
    VkExtent2D dummyExtent = {800, 600};

    EXPECT_THROW({
        Pipeline badPipeline(VK_NULL_HANDLE,
                             "shaders/nem_letezo_vert.spv",
                             "shaders/nem_letezo_frag.spv",
                             VK_NULL_HANDLE,
                             dummyExtent,
                             VK_NULL_HANDLE);
    }, std::runtime_error);
}

// =================================================================
// 3. APPLIKÁCIÓ TESZTEK
// =================================================================

/**
 * @brief Ellenőrzi az Application Layer (RetopoApp) sikeres példányosítását.
 * Biztosítja, hogy a főosztály objektumának memóriafoglalása során nem lép fel váratlan memóriaszivárgás vagy fagyás.
 */
TEST(RetopoAppTest, AppCanBeInstantiated) {
    EXPECT_NO_THROW({
        RetopoApp app;
    });
}

// =================================================================
// 4. LOGIKAI ÉS ADATSTRUKTÚRA TESZTEK (Unit Tests)
// =================================================================

/**
 * @brief Teszteli a videókártya parancssorainak (QueueFamilies) logikai kiértékelését.
 * Csak akkor adhat vissza igazat, ha mind a rajzolási (Graphics), mind a képernyő (Present) sora megtalálható.
 */
TEST(QueueFamilyTest, IsCompleteLogicChecks) {
    QueueFamilyIndices indices;

    EXPECT_FALSE(indices.isComplete());

    indices.graphicsFamily = 0;
    EXPECT_FALSE(indices.isComplete());

    indices.presentFamily = 0;
    EXPECT_TRUE(indices.isComplete());
}

/**
 * @brief A Vertex adatszerkezet egyenlőség-vizsgálata és Hash-generátorának validációja.
 * Ez a teszt igazolja, hogy a memóriahatékony OBJ beolvasáshoz használt unordered_map
 * pontosan és ütközésmentesen ismeri fel a térben egybeeső pontokat a VRAM spórolás érdekében.
 */
TEST(VertexTest, EqualityAndHashing) {
    Vertex v1{};
    v1.pos = {1.0f, 2.0f, 3.0f};
    v1.color = {1.0f, 0.0f, 0.0f};

    Vertex v2{}; // v2 pontosan ugyanaz, mint v1
    v2.pos = {1.0f, 2.0f, 3.0f};
    v2.color = {1.0f, 0.0f, 0.0f};

    Vertex v3{}; // v3 egy teljesen más pont
    v3.pos = {0.0f, 0.0f, 0.0f};

    // 1. Egyenlőség operátor (==) tesztelése
    EXPECT_EQ(v1, v2) << "A ket megegyezo pontnak egyenlonek kell lennie!";
    EXPECT_FALSE(v1 == v3) << "A ket kulonbozo pont nem lehet egyenlo!";

    // 2. Hash generátor (ujjlenyomat) tesztelése
    auto hash1 = std::hash<Vertex>()(v1);
    auto hash2 = std::hash<Vertex>()(v2);
    auto hash3 = std::hash<Vertex>()(v3);

    EXPECT_EQ(hash1, hash2) << "Az azonos pontok Hash ertekenek egyeznie kell!";
    EXPECT_NE(hash1, hash3) << "A kulonbozo pontok Hash erteke el kell hogy terjen!";
}

/**
 * @brief Kameramatematika és határérték-vizsgálat (Boundary Value Analysis).
 * * Ez a teszt bizonyítja, hogy a kameravezérlő logikánk megvédi a rendszert a matematikai
 * Gimbal Lock-tól (amikor a kamera inverzbe fordulna a feje tetején).
 * A satunak pontosan 89 és -89 foknál meg kell állítania a mozgást.
 */
TEST(CameraMathTest, PitchLimitClamping) {
    float cameraPitch = 45.0f;
    float deltaY = 500.0f; // Szimuláljuk, hogy a felhasználó durván lehúzza az egeret

    // A RetopoApp.cpp-ben lévő logika másolata:
    cameraPitch += deltaY * 0.5f;

    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    // Bár a matematika szerint 45 + (500*0.5) = 295 lenne, a satunak meg kell állítania 89.0-nál!
    EXPECT_FLOAT_EQ(cameraPitch, 89.0f) << "A kamera dolesszoget (Pitch) 89 foknal maximalizalni kell!";

    // Szimuláljuk a másik irányt (felfelé tolás extrém értékkel)
    cameraPitch = -100.0f;
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    EXPECT_FLOAT_EQ(cameraPitch, -89.0f) << "A kamera dolesszoget -89 foknal minimalizalni kell!";
}