#pragma once
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "glad/glad.h"

struct SphereVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

class UnlitMaterial;

class Sphere
{
public:
    Sphere(float radius = 1.0f, int sectors = 32, int stacks = 32);
    ~Sphere();

    void Draw() const;
    void SetupMesh();

    std::shared_ptr<UnlitMaterial> GetMaterial();

private:
    void CreateSphere(float radius, int sectors, int stacks);

    std::vector<SphereVertex> vertices;
    std::vector<unsigned int> indices;

    unsigned int VAO, VBO, EBO;
    bool initialized = false;

    glm::vec3 m_Pos{0.0f, 0.0f, 0.0f};
    std::shared_ptr<UnlitMaterial> mpUnlitMaterial{ nullptr };
};
