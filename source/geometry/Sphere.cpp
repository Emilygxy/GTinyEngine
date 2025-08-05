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
    :m_Radius(radius)
{
    m_Pos = glm::vec3(0.0f, 0.0f, -10.0f);
    CreateSphere(sectors, stacks);

    //material
    auto pUnlitMaterial = std::make_shared<PhongMaterial>();
    pUnlitMaterial->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    SetMaterial(pUnlitMaterial);
}

Sphere::~Sphere() {}

void Sphere::SetRadius(float radius)
{
    m_Radius = radius;
}

float Sphere::GetRadius() const noexcept
{
    return m_Radius;
}

void Sphere::CreateSphere(int sectors, int stacks) {
    mVertices.clear();
    mIndices.clear();

    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * stackStep;
        float xy = m_Radius * cosf(stackAngle);
        float z = m_Radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            Vertex vertex;
            vertex.position = /*m_Pos + */glm::vec3(x, y, z);
            vertex.normal = glm::normalize(glm::vec3(x, y, z));
            vertex.texCoords = glm::vec2((float)j / sectors, (float)i / stacks);
            mbHasUV = true;

            mVertices.push_back(vertex);
        }
    }

    // indices
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                mIndices.push_back(k1);
                mIndices.push_back(k2);
                mIndices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                mIndices.push_back(k1 + 1);
                mIndices.push_back(k2);
                mIndices.push_back(k2 + 1);
            }
        }
    }

    SetupMesh();
}