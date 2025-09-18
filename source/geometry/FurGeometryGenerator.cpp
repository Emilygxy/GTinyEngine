#include "geometry/FurGeometryGenerator.h"

#include <glm/gtc/random.hpp>
#include <iostream>

void FurGeometryGenerator::GenerateHairFromBaseMesh(const std::vector<Vertex>& baseVertices, 
                                                   const std::vector<unsigned int>& baseIndices, 
                                                   int numLayers, 
                                                   float hairLength, 
                                                   float hairDensity)
{
    mHairVertices.clear();
    mHairIndices.clear();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    std::cout << "Generating hair from base mesh with " << baseVertices.size() << " vertices and " 
              << baseIndices.size() / 3 << " triangles" << std::endl;
    
    // generate fur for each triangle
    for (size_t i = 0; i < baseIndices.size(); i += 3) {
        const Vertex& v0 = baseVertices[baseIndices[i]];
        const Vertex& v1 = baseVertices[baseIndices[i + 1]];
        const Vertex& v2 = baseVertices[baseIndices[i + 2]];
        
        // calculate triangle area (for density distribution)
        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        float area = 0.5f * glm::length(glm::cross(edge1, edge2));
        
        // calculate how many hairs should be generated for this triangle based on density
        int numHairsInTriangle = static_cast<int>(area * hairDensity * 1000.0f); // 缩放密度
        
        for (int j = 0; j < numHairsInTriangle; ++j) {
            // generate random barycentric coordinates
            float r1 = dis(gen);
            float r2 = dis(gen);
            if (r1 + r2 > 1.0f) {
                r1 = 1.0f - r1;
                r2 = 1.0f - r2;
            }
            float r3 = 1.0f - r1 - r2;
            
            // interpolate position and normal on the triangle
            glm::vec3 startPos = r1 * v0.position + r2 * v1.position + r3 * v2.position;
            glm::vec3 normal = glm::normalize(r1 * v0.normal + r2 * v1.normal + r3 * v2.normal);
            glm::vec2 texCoords = r1 * v0.texCoords + r2 * v1.texCoords + r3 * v2.texCoords;
            
            // generate hair direction (for better natural effect to offset the normal)
            glm::vec3 hairDirection = GenerateHairDirection(normal, gen, dis);
            
            // generate fur geometry for each layer
            for (int layer = 0; layer < numLayers; ++layer) {
                float layerRatio = static_cast<float>(layer) / static_cast<float>(numLayers);
                glm::vec3 currentPos = startPos + hairDirection * hairLength * layerRatio;
                
                // create hair vertex
                Vertex hairVertex;
                hairVertex.position = currentPos;
                hairVertex.normal = normal;
                hairVertex.texCoords = texCoords;
                
                mHairVertices.push_back(hairVertex);
            }
        }
    }
    
    // generate indices (organize fur into triangle strips)
    GenerateHairIndices(numLayers);
    
    std::cout << "Generated " << mHairVertices.size() << " hair vertices and " 
              << mHairIndices.size() / 3 << " hair triangles" << std::endl;
}

glm::vec3 FurGeometryGenerator::GenerateHairDirection(const glm::vec3& surfaceNormal, 
                                                     std::mt19937& gen, 
                                                     std::uniform_real_distribution<float>& dis)
{
    // add some randomness around the normal
    glm::vec3 randomOffset = glm::vec3(
        dis(gen) - 0.5f,
        dis(gen) - 0.5f,
        dis(gen) - 0.5f
    ) * 0.2f; // control the strength of randomness
    
    glm::vec3 direction = glm::normalize(surfaceNormal + randomOffset);
    
    // ensure hair grows outward
    if (glm::dot(direction, surfaceNormal) < 0.0f) {
        direction = -direction;
    }
    
    return direction;
}

void FurGeometryGenerator::GenerateHairIndices(int numLayers)
{
    // organize fur into triangle strips
    // each hair is composed of multiple layers, connected by triangles
    
    int verticesPerHair = numLayers;
    int numHairs = static_cast<int>(mHairVertices.size()) / verticesPerHair;
    
    for (int hairIndex = 0; hairIndex < numHairs; ++hairIndex) {
        int baseIndex = hairIndex * verticesPerHair;
        
        // generate triangles for each layer
        for (int layer = 0; layer < numLayers - 1; ++layer) {
            int currentLayerStart = baseIndex + layer;
            int nextLayerStart = baseIndex + layer + 1;
            
            // create two triangles (if needed, can add width)
            // here simplified, only create basic hair geometry
            if (layer < numLayers - 2) {
                // triangle 1
                mHairIndices.push_back(currentLayerStart);
                mHairIndices.push_back(nextLayerStart);
                mHairIndices.push_back(currentLayerStart + 1);
                
                // triangle 2
                mHairIndices.push_back(nextLayerStart);
                mHairIndices.push_back(nextLayerStart + 1);
                mHairIndices.push_back(currentLayerStart + 1);
            }
        }
    }
}
