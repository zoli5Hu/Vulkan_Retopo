# 3D Retopo Tool (Vulkan Motor)

Ez a projekt egy szakdolgozat keretein belül készülő, Vulkan API alapú 3D retopológiai szoftver alapmotorja. A projekt célja, hogy kihasználja a modern GPU-k számítási és grafikus kapacitását a geometriai hálók (mesh-ek) optimalizálásához.

## 🏗️ Jelenlegi Állapot (Architektúra)

A motor jelenleg a következő stabil, multiplatform (Windows / macOS) alapokkal rendelkezik:

### 1. Ablakkezelés (GLFW)
- A projekt a **GLFW** könyvtárat használja a natív ablakok létrehozására és az operációs rendszer eseményeinek (pl. bezárás) kezelésére[cite: 7].
- A CMake a `FetchContent` segítségével automatikusan letölti és fordítja a GLFW-t, így a kód hordozható[cite: 7].

### 2. Vulkan Inicializálás (`VkInstance`)
- Ez a szoftver és a Vulkan API közötti fő kapcsolat[cite: 7].
- **Validation Layers:** Debug módban automatikusan bekapcsolnak a Khronos hibaellenőrző rétegei, amik azonnal szólnak, ha valami nem a Vulkan specifikációi szerint történik[cite: 7].
- **Portability (Mac):** Apple Silicon / macOS környezetben a rendszer automatikusan betölti a `VK_KHR_portability_enumeration` kiterjesztéseket a MoltenVK futtatásához[cite: 7].

### 3. Ablak Felszín (`VkSurfaceKHR`)
- A natív híd a Vulkan motor és az operációs rendszer (ablakkezelő) között[cite: 7].
- Ez az a "vászon", ahová a videókártya a végleges, kiszámolt képeket továbbítja. A GLFW automatikusan lekezeli a platformspecifikus (Windows/Mac) eltéréseket a létrehozásakor[cite: 7].

### 4. Hardver Kiválasztása (`VkPhysicalDevice`)
- A program lekérdezi a gépben lévő összes videókártyát[cite: 7].
- Megvizsgálja a kártyák "részlegeit" (**Queue Families**), és kiválaszt egy olyan hardvert, ami rendelkezik grafikus renderelésre alkalmas számítási sorral (`VK_QUEUE_GRAPHICS_BIT`), **ÉS** támogatja a megjelenítést a létrehozott ablakhidunkon (`presentSupport`)[cite: 7].

### 5. Kapcsolat a hardverrel (`VkDevice` és `VkQueue`)
- **Logical Device (`VkDevice`):** Ez a "szerződés" a kiválasztott videókártyával. A program hátralévő részében a memóriafoglalások, shaderek és rajzolási parancsok ezen a logikai eszközön keresztül futnak[cite: 7].
- **Graphics Queue:** A "munkások futószalagja", amin keresztül a program beküldi a grafikus számítási parancsokat a GPU-ba[cite: 7].
- **Present Queue:** A "kézbesítő futószalag", ami az elkészült képeket kiküldi a `VkSurfaceKHR` felületre, hogy megjelenjenek a képernyőn. A rendszer okosan (egy `std::set` segítségével) felismeri, ha a rajzoló és a megjelenítő chip ugyanaz, így elkerüli a felesleges memóriafoglalást[cite: 7].

### 6. Csere-lánc (Swap Chain - `VkSwapchainKHR`)
- Ez a videókártyán lévő, renderelésre váró vásznak (képek) sorozata (Double / Triple Buffering).
- **Hardveres lekérdezés:** A program a "Vulkan kétlépéses lekérdezésével" (Two-Step Query) biztonságosan feltérképezi a monitor képességeit, a támogatott felbontásokat és formátumokat, így optimalizálva a memóriahasználatot.
- **Megjelenítési módok:** A motor törekszik a legjobb teljesítményt nyújtó `MAILBOX` (Triple Buffering) módra, de zökkenőmentesen vissza tud váltani a szabványos `FIFO` (V-Sync) módra, hogy elkerülje a képtörést (screen tearing).

## 📁 Projekt Struktúra
```text
Vulkan_Retopo/
 ├── CMakeLists.txt      # Multiplatform build konfiguráció[cite: 7]
 ├── README.md           # Projekt leírás és architektúra[cite: 7]
 ├── main.cpp            # Belépési pont[cite: 7]
 └── src/                # Forráskód[cite: 7]
      ├── RetopoApp.h    # Az applikáció deklarációi (A "tartalomjegyzék")[cite: 7]
      └── RetopoApp.cpp  # A Vulkan és ablakkezelés logikája[cite: 7]