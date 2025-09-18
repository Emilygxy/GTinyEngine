#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "mesh/Vertex.h"
#include <random>

struct HairStrand {
    glm::vec3 startPos;
    glm::vec3 endPos;
    glm::vec3 tangent;
    float radius;
    glm::vec3 color;
};

class FurGeometryGenerator {
public:
    void GenerateHairFromBaseMesh(const std::vector<Vertex>& baseVertices, 
                                 const std::vector<unsigned int>& baseIndices,
                                 int numLayers = 8,
                                 float hairLength = 0.1f,
                                 float hairDensity = 0.5f);
    
    const std::vector<Vertex>& GetHairVertices() const { return mHairVertices; }
    const std::vector<unsigned int>& GetHairIndices() const { return mHairIndices; }

private:
    std::vector<Vertex> mHairVertices;
    std::vector<unsigned int> mHairIndices;
    
    // 辅助方法
    glm::vec3 GenerateHairDirection(const glm::vec3& surfaceNormal, 
                                   std::mt19937& gen, 
                                   std::uniform_real_distribution<float>& dis);
    void GenerateHairIndices(int numLayers);
};