#include "MeshAnalyzer.h"
#include <cmath>
#include <limits>
#include <algorithm>

float MeshAnalyzer::sigma(float avg, const std::vector<float> &numbers) {
    if (numbers.empty()) {
        return 0.0f;
    }
    float varianceSum = 0.0f;
    for (const auto &num : numbers) {
        float diff = num - avg;
        varianceSum += diff * diff;
    }
    return std::sqrt(varianceSum / numbers.size());
}

std::unordered_map<Vertex, std::unordered_map<Vertex, std::vector<float> > > MeshAnalyzer::simpledistance(
    const std::vector<Vertex> &highPolyVertices, std::vector<Vertex> &lowPolyVertices, float sigmaMultiplier) {
    std::unordered_map<Vertex, std::unordered_map<Vertex, std::vector<float> > > result;
    float avg = 0.0f;
    if (highPolyVertices.empty() || lowPolyVertices.empty()) {
        return result;
    }

    for (auto &highPolyVertex : highPolyVertices) {
        float minDistancec = std::numeric_limits<float>::max();

        Vertex closestVertex;
        for (auto &lowPolyVertex : lowPolyVertices) {
            float distance = glm::distance(highPolyVertex.pos, lowPolyVertex.pos);
            if (minDistancec > distance) {
                minDistancec = distance;
                closestVertex = lowPolyVertex;
            }
        }
        result[closestVertex][highPolyVertex].push_back(minDistancec);
    }

    // 1. Minden Voronoi-régióra kiszámoljuk a régió súlypontjának (centroid) távolságát a Low Poly csúcstól
    std::unordered_map<Vertex, float> regionDistances;
    std::vector<float> allRegionDistances;
    allRegionDistances.reserve(result.size());

    for (const auto &regionPair : result) {
        const Vertex &lowVertex = regionPair.first;
        glm::vec3 sulyPont(0.0f);
        for (const auto &highPair : regionPair.second) {
            sulyPont += highPair.first.pos; // A régióba tartozó High Poly pontok pozícióinak összeadása
        }
        sulyPont /= static_cast<float>(regionPair.second.size()); // Súlypont (átlagos pozíció) a régióban

        float pukliDist = glm::distance(lowVertex.pos, sulyPont);
        regionDistances[lowVertex] = pukliDist;
        allRegionDistances.push_back(pukliDist);
        avg += pukliDist;
    }

    if (!allRegionDistances.empty()) {
        avg /= static_cast<float>(allRegionDistances.size());
    }

    // 2. A régiók súlypont-távolságaiból számoljuk a szórást (sigma) és a határt
    float stdDev = sigma(avg, allRegionDistances);
    float sigmabordertop = avg + (sigmaMultiplier * stdDev);

    // 3. Színezés
    for (auto & color : lowPolyVertices) { // 1 low alap vertex
        float maxDist = 0.0f;
        auto it = regionDistances.find(color);

        if (it != regionDistances.end()) {
            maxDist = it->second;
        } else {
            // Ha nincs saját Voronoi-régiója (kilógó pont), megnézzük a hozzá legközelebbi High Poly pont távolságát
            maxDist = std::numeric_limits<float>::max();
            for (const auto &highPolyVertex : highPolyVertices) {
                float dist = glm::distance(color.pos, highPolyVertex.pos);
                if (dist < maxDist) {
                    maxDist = dist;
                }
            }
        }

        if (sigmabordertop > 0.0f) {
            if (maxDist <= sigmabordertop) {
                // Ha a szórás tartományán belül van, 0 és sigmabordertop között normalizáljuk (0.0 = zöld, 1.0 = piros)
                float normalizedValue = glm::clamp(maxDist / sigmabordertop, 0.0f, 1.0f);
                color.color = glm::vec3(normalizedValue, 1.0f - normalizedValue, 0.0f); // Zöld -> Piros skála
            } else {
                // Ha a szigmás határon kívül van, lilára színezzük (kiugró érték / outlier)
                color.color = glm::vec3(1.0f, 0.0f, 1.0f); // Lila szín
            }
        } else {
            color.color = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    return result;
}
