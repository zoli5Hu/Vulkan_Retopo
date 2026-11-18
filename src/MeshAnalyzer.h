#pragma once

#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include "ResourceManager.h"

/**
 * @class MeshAnalyzer
 * @brief A High Poly és Low Poly modellek közötti felületi eltérések (puklik),
 * Voronoi-régiók és hőtérképes (heatmap) színezés számításáért felelős modul.
 */
class MeshAnalyzer {
public:
    MeshAnalyzer() = default;
    ~MeshAnalyzer() = default;
    //egyszerű összehasonlítás távolságra
    static std::unordered_map<Vertex, std::unordered_map<Vertex, std::vector<float>>> simpledistance(const std::vector<Vertex> &highPolyVertices, std::vector<Vertex> &lowPolyVertices, float sigmaMultiplier = 3.0f);
    static float sigma(float avg, const std::vector<float> &numbers);
};
