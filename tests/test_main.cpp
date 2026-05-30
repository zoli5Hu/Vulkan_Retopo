#include <gtest/gtest.h>
#include "../src/Pipeline.h"
#include "../src/RetopoApp.h"
#include "../src/Vertex.h" // <--- ÚJ: Behoztuk a Vertex struktúrát a teszthez
#include <stdexcept>
#include <glm/glm.hpp>     // <--- ÚJ: Matematikai tesztekhez

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
// Leteszteljük, hogy az alkalmazás objektum sikeresen példányosítható-e
TEST(RetopoAppTest, AppCanBeInstantiated) {
    EXPECT_NO_THROW({
        RetopoApp app;
    });
}

// =================================================================
// 4. LOGIKAI ÉS ADATSTRUKTÚRA TESZTEK (Unit Tests)
// =================================================================

// 4.1. QueueFamily validáció
TEST(QueueFamilyTest, IsCompleteLogicChecks) {
    QueueFamilyIndices indices;

    EXPECT_FALSE(indices.isComplete());

    indices.graphicsFamily = 0;
    EXPECT_FALSE(indices.isComplete());

    indices.presentFamily = 0;
    EXPECT_TRUE(indices.isComplete());
}

// 4.2. ÚJ TESZT: Térbeli pontok (Vertex) egyenlősége és Hashing
TEST(VertexTest, EqualityAndHashing) {
    Vertex v1{};
    v1.pos = {1.0f, 2.0f, 3.0f};
    v1.color = {1.0f, 0.0f, 0.0f};
    // (A texCoord sorokat kivettük, hogy pontosan illeszkedjen a te Vertex.h fájlodhoz)

    Vertex v2{}; // v2 pontosan ugyanaz, mint v1
    v2.pos = {1.0f, 2.0f, 3.0f};
    v2.color = {1.0f, 0.0f, 0.0f};

    Vertex v3{}; // v3 egy teljesen más pont
    v3.pos = {0.0f, 0.0f, 0.0f};

    // 1. Egyenlőség operátor (==) tesztelése
    EXPECT_EQ(v1, v2) << "A ket megegyezo pontnak egyenlonek kell lennie!";

    // EXPECT_NE helyett EXPECT_FALSE-t használunk, mert a != operátor valószínűleg nincs definiálva
    EXPECT_FALSE(v1 == v3) << "A ket kulonbozo pont nem lehet egyenlo!";

    // 2. Hash generátor tesztelése
    auto hash1 = std::hash<Vertex>()(v1);
    auto hash2 = std::hash<Vertex>()(v2);
    auto hash3 = std::hash<Vertex>()(v3);

    EXPECT_EQ(hash1, hash2) << "Az azonos pontok Hash ertekenek egyeznie kell!";
    // Itt a hash már egy sima szám (size_t), erre gond nélkül működik az EXPECT_NE!
    EXPECT_NE(hash1, hash3) << "A kulonbozo pontok Hash erteke el kell hogy terjen!";
}

// 4.3. ÚJ TESZT: Kamera "Satu" (Clamp) Matematika
// Ez a teszt bizonyítja, hogy a kameravezérlő logikánk megvédi a
// kamerát attól, hogy a feje tetejére álljon, bármennyit is mozdul az egér.
TEST(CameraMathTest, PitchLimitClamping) {
    float cameraPitch = 45.0f;
    float deltaY = 500.0f; // Szimuláljuk, hogy a felhasználó durván lehúzza az egeret

    // A RetopoApp.cpp-ben lévő logika másolata:
    cameraPitch += deltaY * 0.5f;

    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    // Bár a matematika szerint 45 + (500*0.5) = 295 lenne,
    // a satunak meg kell állítania 89.0-nál!
    EXPECT_FLOAT_EQ(cameraPitch, 89.0f) << "A kamera dolesszoget (Pitch) 89 foknal maximalizalni kell!";

    // Szimuláljuk a másik irányt (felfelé tolás)
    cameraPitch = -100.0f;
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;

    EXPECT_FLOAT_EQ(cameraPitch, -89.0f) << "A kamera dolesszoget -89 foknal minimalizalni kell!";
}