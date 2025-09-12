#pragma once
#include <vector>
#include <string>
#include <memory>
#include "geometry/BasicGeometry.h"

class Sphere : public BasicGeometry
{
public:
    Sphere(float radius = 1.0f, int sectors = 32, int stacks = 32);
    ~Sphere();

    void SetRadius(float radius);
    float GetRadius() const noexcept;

private:
    void CreateSphere(int sectors, int stacks);

    float m_Radius{ 1.0f };
    glm::vec3 m_Pos{0.0f, 0.0f, 0.0f};
};
