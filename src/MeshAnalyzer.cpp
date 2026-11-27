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

std::unordered_map<Vertex, std::unordered_map<Vertex, float > > MeshAnalyzer::simpledistance(
    const std::vector<Vertex> &highPolyVertices, std::vector<Vertex> &lowPolyVertices, float sigmaMultiplier, bool colorizeOn) {
    std::unordered_map<Vertex, std::unordered_map<Vertex, float > > result;
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
        result[closestVertex][highPolyVertex] = minDistancec;
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
        Vertex closestHigh = Vertex();
        if (it != regionDistances.end()) {
            maxDist = it->second;
        } else {
            // Ha nincs saját Voronoi-régiója (kilógó pont), megnézzük a hozzá legközelebbi High Poly pont távolságát
            maxDist = std::numeric_limits<float>::max();
            for (const auto &highPolyVertex : highPolyVertices) {
                float dist = glm::distance(color.pos, highPolyVertex.pos);
                if (dist < maxDist) {
                    maxDist = dist;
                    closestHigh = highPolyVertex;

                }
            }
        }

        if (sigmabordertop > 0.0f) {
            if (maxDist <= sigmabordertop) {
                // Ha a szórás tartományán belül van, 0 és sigmabordertop között normalizáljuk (0.0 = zöld, 1.0 = piros)
                float normalizedValue = glm::clamp(maxDist / sigmabordertop, 0.0f, 1.0f);
                if (colorizeOn) {
                    color.color = glm::vec3(normalizedValue, 1.0f - normalizedValue, 0.0f); // Zöld -> Piros skála
                }
            } else {
                // Ha a szigmás határon kívül van, lilára színezzük (kiugró érték / outlier)
                if (colorizeOn) {
                    color.color = glm::vec3(1.0f, 0.0f, 1.0f); // Lila szín
                }
                if (it == regionDistances.end()) {
                    result[color][closestHigh] = maxDist;
                }
            }
        } else {

            color.color = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    return result;
}
// low vertex , high vertex , lowvertex távolság hightól
bool MeshAnalyzer::shrinkwrap(std::unordered_map<Vertex, std::unordered_map<Vertex, float>> distances, std::vector<Vertex> &lowPolyVertex) {
    if (distances.empty() || lowPolyVertex.empty()) {
        return false;
    }
    for (auto &original_lowpoly : lowPolyVertex) {
        auto it = distances.find(original_lowpoly);
        if (it != distances.end()) {
            float min = std::numeric_limits<float>::max();
            glm::vec3 minVec3;
            for (auto & df : it->second) {
                if (df.second < min) {
                    min = df.second;
                    minVec3 = df.first.pos;
                }
            }
            original_lowpoly.pos = minVec3;

        }
    }
    return true;
};

bool MeshAnalyzer::relax(std::vector<Vertex> &lowPolyVertices, const std::vector<uint32_t> &lowPolyIndices, float factor) {

    if (lowPolyVertices.empty() || lowPolyIndices.empty()) {
        return false;
    }

    std::unordered_map<uint32_t, std::vector<uint32_t>> neighbors;
    for (size_t i = 0; i + 2 < lowPolyIndices.size(); i += 3) {
        uint32_t idx0 = lowPolyIndices[i];
        uint32_t idx1 = lowPolyIndices[i + 1];
        uint32_t idx2 = lowPolyIndices[i + 2];

        neighbors[idx0].push_back(idx1);
        neighbors[idx0].push_back(idx2);
        neighbors[idx1].push_back(idx0);
        neighbors[idx1].push_back(idx2);
        neighbors[idx2].push_back(idx0);
        neighbors[idx2].push_back(idx1);
    }

    std::vector<Vertex> originalVertices = lowPolyVertices;
    for (auto &s : neighbors) {
        glm::vec3 avg = glm::vec3(0.0f);
        for (auto n : s.second) {
            avg += originalVertices[n].pos;
        }
        avg /= static_cast<float>(s.second.size());
        lowPolyVertices[s.first].pos = glm::mix(originalVertices[s.first].pos, avg, factor);
    }
    return true;
}


