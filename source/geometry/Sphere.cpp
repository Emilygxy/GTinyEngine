#include "geometry/Sphere.h"
#include <cmath>
#include <iostream>
#include "ultis.h"
#include "materials/BaseMaterial.h"

namespace
{
    const float M_PI = 3.14159265358979323846f;
}

Sphere::Sphere(float radius, int sectors, int stacks)
    :mpUnlitMaterial(std::make_shared<UnlitMaterial>())
{
    m_Pos = glm::vec3(0.0f, 0.0f, -10.0f);
    CreateSphere(radius, sectors, stacks);

    mpUnlitMaterial->SetTexturePath("resources/textures/IMG_8515.JPG");
}

Sphere::~Sphere() {
    if (initialized) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
}

void Sphere::CreateSphere(float radius, int sectors, int stacks) {
    vertices.clear();
    indices.clear();

    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * stackStep;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            SphereVertex vertex;
            vertex.position = /*m_Pos + */glm::vec3(x, y, z);
            vertex.normal = glm::normalize(glm::vec3(x, y, z));
            vertex.texCoords = glm::vec2((float)j / sectors, (float)i / stacks);

            vertices.push_back(vertex);
        }
    }

    // indices
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    SetupMesh();
}

std::shared_ptr<UnlitMaterial> Sphere::GetMaterial()
{
    return mpUnlitMaterial;
}

void Sphere::SetupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SphereVertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)offsetof(SphereVertex, normal));

    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SphereVertex), (void*)offsetof(SphereVertex, texCoords));

    glBindVertexArray(0);
    initialized = true;
}

void Sphere::Draw() const {
    if (!initialized) return;

    // bind texture
    mpUnlitMaterial->OnBind();

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, GLsizei(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}