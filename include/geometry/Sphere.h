#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "glad/glad.h"

struct SphereVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};


class Sphere
{
public:
    Sphere(float radius = 1.0f, int sectors = 32, int stacks = 32);
    ~Sphere();

    void Draw() const;
    void SetupMesh();

    void SetTexturePath(const std::string& path);

private:
    void CreateSphere(float radius, int sectors, int stacks);
    void CalculateTangentBasis();

    //texture 
    void SetTexture(unsigned int textureID);
    unsigned int GetTexture() const { return textureID; }
    bool HasTexture() const { return hasTexture; }

    std::vector<SphereVertex> vertices;
    std::vector<unsigned int> indices;

    unsigned int VAO, VBO, EBO;
    bool initialized = false;

    unsigned int textureID;
    bool hasTexture = false;

    glm::vec3 m_Pos{0.0f, 0.0f, 0.0f};

};
