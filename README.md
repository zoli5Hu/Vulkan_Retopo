# 3D Retopo Tool (Vulkan Motor)

Ez a projekt egy szakdolgozat keretein belül készülő, Vulkan API alapú 3D retopológiai szoftver alapmotorja. A projekt célja, hogy kihasználja a modern GPU-k számítási és grafikus kapacitását a geometriai hálók (mesh-ek) optimalizálásához.

## 🏗️ Jelenlegi Állapot (Architektúra)

A motor jelenleg a következő stabil, multiplatform (Windows / macOS) alapokkal rendelkezik:

### 1. Ablakkezelés (GLFW)
- A projekt a **GLFW** könyvtárat használja a natív ablakok létrehozására és az operációs rendszer eseményeinek (pl. bezárás) kezelésére.
- A CMake a `FetchContent` segítségével automatikusan letölti és fordítja a GLFW-t, így a kód hordozható.

### 2. Vulkan Inicializálás (`VkInstance`)
- Ez a szoftver és a Vulkan API közötti fő kapcsolat.
- **Validation Layers:** Debug módban automatikusan bekapcsolnak a Khronos hibaellenőrző rétegei, amik azonnal szólnak, ha valami nem a Vulkan specifikációi szerint történik.
- **Portability (Mac):** Apple Silicon / macOS környezetben a rendszer automatikusan betölti a `VK_KHR_portability_enumeration` kiterjesztéseket a MoltenVK futtatásához.

### 3. Hibakereső Rendszer (Debug Messenger - `VkDebugUtilsMessengerEXT`)
- A Validation Layerek hivatalos kiterjesztése, ami egy egyedi visszahívó (callback) függvényen keresztül elkapja a Vulkan belső üzeneteit.
- Garantálja, hogy a helytelen API-használat (pl. rossz törlési sorrend, memóriaszivárgás) azonnal, jól olvasható konzolos hibaüzenetként jelenjen meg, megelőzve a csendes összeomlásokat (Segmentation Fault).

### 4. Ablak Felszín (`VkSurfaceKHR`)
- A natív híd a Vulkan motor és az operációs rendszer (ablakkezelő) között.
- Ez az a "vászon", ahová a videókártya a végleges, kiszámolt képeket továbbítja. A GLFW automatikusan lekezeli a platformspecifikus (Windows/Mac) eltéréseket a létrehozásakor.

### 5. Hardver Kiválasztása (`VkPhysicalDevice`)
- A program lekérdezi a gépben lévő összes videókártyát.
- Megvizsgálja a kártyák "részlegeit" (**Queue Families**), és kiválaszt egy olyan hardvert, ami rendelkezik grafikus renderelésre alkalmas számítási sorral (`VK_QUEUE_GRAPHICS_BIT`), **ÉS** támogatja a megjelenítést a létrehozott ablakhidunkon (`presentSupport`).

### 6. Kapcsolat a hardverrel (`VkDevice` és `VkQueue`)
- **Logical Device (`VkDevice`):** Ez a "szerződés" a kiválasztott videókártyával. A program hátralévő részében a memóriafoglalások, shaderek és rajzolási parancsok ezen a logikai eszközön keresztül futnak.
- **Graphics Queue:** A "munkások futószalagja", amin keresztül a program beküldi a grafikus számítási parancsokat a GPU-ba.
- **Present Queue:** A "kézbesítő futószalag", ami az elkészült képeket kiküldi a `VkSurfaceKHR` felületre, hogy megjelenjenek a képernyőn. A rendszer okosan (egy `std::set` segítségével) felismeri, ha a rajzoló és a megjelenítő chip ugyanaz, így elkerüli a felesleges memóriafoglalást.

### 7. Csere-lánc (Swap Chain - `VkSwapchainKHR`)
- Ez a videókártyán lévő, renderelésre váró vásznak (képek) sorozata (Double / Triple Buffering).
- **Hardveres lekérdezés:** A program a "Vulkan kétlépéses lekérdezésével" (Two-Step Query) biztonságosan feltérképezi a monitor képességeit, a támogatott felbontásokat és formátumokat, így optimalizálva a memóriahasználatot.
- **Megjelenítési módok:** A motor törekszik a legjobb teljesítményt nyújtó `MAILBOX` (Triple Buffering) módra, de zökkenőmentesen vissza tud váltani a szabványos `FIFO` (V-Sync) módra, hogy elkerülje a képtörést (screen tearing).

### 8. Képnézetek (Image Views - `VkImageView`)
- A Vulkan nem tud közvetlenül a nyers memóriába (képekre) rajzolni. Ehhez Image View-kat, azaz "lencséket" használ.
- A motor minden egyes Swap Chain képhez automatikusan generál egy 2D-s, RGB színformátumú nézetet, felkészítve azokat a renderelési parancsok (Graphics Pipeline) fogadására.

### 9. Shaderek és Grafikus Futószalag (Graphics Pipeline)
- **GLSL és SPIR-V:** A motor beolvassa a Vertex és Fragment shadereket, amelyeket a CMake automatikusan a GPU számára olvasható SPIR-V bináris kódra fordít a `glslc` segítségével.
- **Shader Modulok (`VkShaderModule`):** A beolvasott bináris kódból a rendszer Vulkan shader modulokat épít, amik a grafikus futószalag logikáját adják.
- **Biztonságos Memóriakezelés:** Az objektum-orientált felépítés (külön `Pipeline` osztály) és a modern C++ okos mutatók (`std::unique_ptr`) garantálják a stabil élettartam-kezelést a Vulkan szigorú megsemmisítési sorrendjéhez igazodva.

### 10. Render Pass (Megjelenítési Terv)
- Definiálja a rajzolási folyamat szabályait: milyen formátumba rajzolunk, és mit tegyen a GPU a képkeret elején (törlés) és végén (mentés).

### 11. Framebufferek (Képkeret-tartályok)
- Fizikailag összekapcsolják a Swap Chain képeit a Render Pass-szal. Minden egyes képhez létrejön egy saját framebuffer, amely tartályként szolgál a renderelési műveletekhez.

### 12. Szinkronizációs architektúra (Lámpák és Sorompók)
- **Semaphores (GPU-GPU):** Koordinálják a műveleteket a videókártyán belül. Az `imageAvailableSemaphore` megvárja, amíg a monitor felszabadít egy vásznat, a `renderFinishedSemaphore` pedig jelzi, ha a rajzolás befejeződött.
- **Fences (CPU-GPU):** Szabályozzák a processzor sebességét. Az `inFlightFence` megakadályozza, hogy a CPU új parancsokat küldjön, amíg a GPU nem végzett az előző képkockával.
- **Frames in Flight:** A motor egyszerre több (2) képkockát tart a "csőben", így amíg a GPU rajzol, a CPU már előkészítheti a következő parancsokat, maximalizálva a teljesítményt.

### 13. Fő Renderelési Ciklus (Main Loop)
- A motor folyamatosan ismétli a következő lépéseket:
    1. Sorompó megvárása (Fence).
    2. Szabad kép kérése a Swap Chain-től.
    3. Parancsok rögzítése (Recording).
    4. Csomag beküldése a GPU-nak (Submit).
    5. Kész kép visszaküldése a monitornak (Present).


### 14. Adatstruktúrák és Memóriakezelés (Vertex Buffers)
A motor egy teljesen adatvezérelt (data-driven) architektúrát használ a 3D geometria kezelésére, kiiktatva a hardkódolt shader adatokat.
- **GLM Integráció:** A csúcspontok (Vertexek) matematikai reprezentációja a `GLM` (OpenGL Mathematics) könyvtárra épül, amely biztosítja a gyors és szabványos 2D/3D vektoros műveleteket (`glm::vec2`, `glm::vec3`).
- **Dedikált GPU Memória (VRAM):** A modell adatait a rendszer a C++ memóriából (RAM) a videókártya saját, hipergyors memóriájába (VRAM) mozgatja át egy `VkBuffer` objektum segítségével.
- **Memória Térképezés (Mapping):** A CPU és GPU közötti kommunikáció a `vkMapMemory` használatával történik, amely "Host Visible" és "Host Coherent" memóriablokkokat keres, így biztosítva az azonnali és szinkronizált adatmásolást (Zero-copy jellegű működés).
- **Dinamikus Tervrajz (Pipeline Input):** A grafikus futószalag (Graphics Pipeline) automatikusan kiolvassa a C++ `Vertex` struktúrából az adatok méretét (Binding Description) és elhelyezkedését (Attribute Descriptions), így a motor könnyedén bővíthető új adatokkal (pl. UV koordináták, normálvektorok) anélkül, hogy a motor alapjait újra kellene írni.

### 15. 3D Tér, Kamera és Uniform Bufferek (MVP)
A motor a 2D-s síkból átlépett a teljes értékű 3D-s térbe, bevezetve a virtuális kamerát és a valós idejű transzformációkat.
- **Valódi 3D Koordináták:** A `Vertex` struktúra kibővült a Z (mélység) tengellyel (`glm::vec3`), lehetővé téve a térbeli objektumok ábrázolását.
- **MVP Mátrixok (Kamera és Vetítés):** A rendszer a standard Model-View-Projection (MVP) mátrix-matematikát használja. A `Model` mátrix a térbeli elhelyezkedésért és forgásért, a `View` a kamera pozíciójáért, a `Projection` pedig a 3D-s tér 2D-s monitorra történő perspektivikus vetítéséért (Raszterizálás) felel.
- **Uniform Bufferek (UBO):** Az állandó (képkockánként nem, de keretenként változó) adatok, mint a kamera állapota, egy speciális GPU memóriába, a Uniform Bufferbe kerülnek. Ezt a memóriát a CPU folyamatosan nyitva tartja (`vkMapMemory`), hogy másodpercenként 60-szor frissíthesse a forgási és kameraadatokat.
- **Descriptor Set-ek és Layout-ok:** A Vulkan szigorú architektúrájának megfelelően a C++ memória (UBO) és a Shader (Pipeline) közötti adatcsere dedikált "adathidakon", Descriptor Set-eken keresztül történik, amiket a rajzolás pillanatában kötünk rá a futószalagra (`vkCmdBindDescriptorSets`).
- **Párhuzamos feldolgozás (Adatkollízió védelem):** A kamera memóriafoglalása képkockánként (Frames in Flight) duplikálva van. Amíg a videókártya (GPU) az egyik memóriablokkból olvassa a kamerát a rajzoláshoz, a processzor (CPU) a másik blokkba már a következő képkocka adatait számolja, elkerülve a képtörést (Screen Tearing) és a szinkronizációs fagyásokat.

## 📁 Projekt Struktúra
```text
Vulkan_Retopo/
 ├── CMakeLists.txt      # Multiplatform build konfiguráció és shader fordítás
 ├── README.md           # Projekt leírás és architektúra
 ├── main.cpp            # Belépési pont
 ├── shaders/            # GLSL shader forráskódok
 │    ├── shader.vert    # Vertex shader
 │    └── shader.frag    # Fragment shader
 └── src/                # Forráskód
      ├── RetopoApp.h/cpp # Fő Vulkan motor és ablakkezelés
      ├── Pipeline.h/cpp  # Grafikus futószalag és shader betöltés
      └── Vertex.h        # C++ csúcspont struktúra és GLM matematika