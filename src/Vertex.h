#pragma once

#include <glm/glm.hpp>
// A GLM hash funkcióinak bekapcsolása a térbeli vektorok ujjlenyomatának generálásához
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.h>
#include <array>

/**
 * @struct Vertex
 * @brief Egyetlen 3D térbeli pontot (csúcspontot) leíró adatstruktúra.
 * * Ez az adatszerkezet határozza meg, hogy milyen információkat küldünk át a CPU-ról
 * a GPU-nak a rajzoláshoz. Jelenleg a térbeli pozíciót és a vertex színét tartalmazza.
 */
struct Vertex {
    glm::vec3 pos;   /**< @brief A pont 3D koordinátája (x, y, z). */
    glm::vec3 color; /**< @brief A pont RGB színe (r, g, b). */

    /**
     * @brief Egyenlőség operátor (==) felüldefiniálása.
     * Megtanítja a C++-t, hogy két Vertex pontosan mikor egyenlő (ha a térbeli helyzetük és színük is egyezik).
     * Ez elengedhetetlen a Hash-map alapú duplikáció-szűréshez az OBJ betöltéskor.
     */
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color;
    }

    /**
     * @brief Megmondja a Vulkan-nak, hogy memóriaszinten mekkora egy "lépésköz" (stride).
     * Egy Vertex-enként lépünk előre az adatok olvasásakor a GPU memóriájában.
     * @return VkVertexInputBindingDescription A memóriakötés (binding) fizikai leírása.
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    /**
     * @brief Részletesen leírja a GPU számára a Vertexen belüli adatok elhelyezkedését.
     * Megmutatja a shadernek, hogy hol kezdődik a pozíció (location 0) és a szín (location 1) a struktúrán belül.
     * @return std::array<VkVertexInputAttributeDescription, 2> Az attribútumok listája.
     */
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // 0. Attribútum: Pozíció (vec3)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // 3 darab 32-bites lebegőpontos szám
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // 1. Attribútum: Szín (vec3)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

/**
 * @brief Szabványos std::hash kiterjesztés a Vertex struktúrához.
 * * Létrehoz egy egyedi matematikai "ujjlenyomatot" (Hash-t) minden egyes térbeli pont számára.
 * A bitenkénti műveletek (XOR és bit-eltolások) biztosítják a gyors és ütközésmentes azonosítást,
 * aminek köszönhetően a ResourceManager-ben lévő unordered_map villámgyorsan ki tudja szűrni a
 * duplikált csúcspontokat, ezáltal drasztikusan csökkentve a VRAM használatot.
 */
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            // A pozíció és a szín vektorainak hash-értékeit kombináljuk össze egyetlen egyedi azonosítóvá
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1);
        }
    };
}