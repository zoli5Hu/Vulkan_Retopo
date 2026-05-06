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

### 3. Ablak Felszín (`VkSurfaceKHR`)
- A natív híd a Vulkan motor és az operációs rendszer (ablakkezelő) között.
- Ez az a "vászon", ahová a videókártya a végleges, kiszámolt képeket továbbítja. A GLFW automatikusan lekezeli a platformspecifikus (Windows/Mac) eltéréseket a létrehozásakor.

### 4. Hardver Kiválasztása (`VkPhysicalDevice`)
- A program lekérdezi a gépben lévő összes videókártyát.
- Megvizsgálja a kártyák "részlegeit" (**Queue Families**), és kiválaszt egy olyan hardvert, ami rendelkezik grafikus renderelésre alkalmas számítási sorral (`VK_QUEUE_GRAPHICS_BIT`), **ÉS** támogatja a megjelenítést a létrehozott ablakhidunkon (`presentSupport`).

### 5. Kapcsolat a hardverrel (`VkDevice` és `VkQueue`)
- **Logical Device (`VkDevice`):** Ez a "szerződés" a kiválasztott videókártyával. A program hátralévő részében a memóriafoglalások, shaderek és rajzolási parancsok ezen a logikai eszközön keresztül futnak.
- **Graphics Queue:** A "munkások futószalagja", amin keresztül a program beküldi a grafikus számítási parancsokat a GPU-ba.
- **Present Queue:** A "kézbesítő futószalag", ami az elkészült képeket kiküldi a `VkSurfaceKHR` felületre, hogy megjelenjenek a képernyőn. A rendszer okosan (egy `std::set` segítségével) felismeri, ha a rajzoló és a megjelenítő chip ugyanaz, így elkerüli a felesleges memóriafoglalást.

## 📁 Projekt Struktúra
```text
Vulkan_Retopo/
 ├── CMakeLists.txt      # Multiplatform build konfiguráció
 ├── README.md           # Projekt leírás és architektúra
 ├── main.cpp            # Belépési pont
 └── src/                # Forráskód
      ├── RetopoApp.h    # Az applikáció deklarációi (A "tartalomjegyzék")
      └── RetopoApp.cpp  # A Vulkan és ablakkezelés logikája