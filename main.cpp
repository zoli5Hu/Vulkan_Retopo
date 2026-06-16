/**
* @file main.cpp
 * @brief Az alkalmazás belépési pontja (Entry Point).
 * * Ez a függvény kezeli az alkalmazás életciklusának elindítását és a kritikus,
 * rendszerszintű hibák elkapását. Ha a Vulkan vagy az operációs rendszer hibát dob,
 * az itt elkapott kivétel (exception) megakadályozza a program "csendes" összeomlását.
 */

#include "src/RetopoApp.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>

/**
 * @brief A program indítása.
 * Létrehozza a `RetopoApp` objektumot, és megkísérli futtatni a grafikus motort.
 * * @return EXIT_SUCCESS, ha a program hibamentesen lefutott.
 * @return EXIT_FAILURE, ha bármilyen kritikus hiba (exception) történt.
 */
int main() {
    // Az alkalmazás példányosítása (A RetopoApp kezeli a Vulkan kontextust és az ablakot)
    RetopoApp app;

    try {
        // Az alkalmazás teljes életciklusának futtatása
        app.run();
    } catch (const std::exception& e) {
        // Ha valami elromlik (pl. nem találja a GPU-t, vagy Shader hibát dob),
        // itt írjuk ki a hibaüzenetet a felhasználónak.
        std::cerr << "KRITIKUS HIBA: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}