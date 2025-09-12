#pragma once
#include <vector>
#include <string>
#include <memory>
#include "geometry/BasicGeometry.h"

class Torus : public BasicGeometry
{
public:
    Torus(float majorRadius = 1.0f, float minorRadius = 0.3f, int majorSegments = 32, int minorSegments = 16);
    ~Torus();

    void setMajorRadius(float majorRadius);
    void setMinorRadius(float minorRadius);
    void setNumMajor(int numMajor);
    void setNumMinor(int numMinor);

    float getMajorRadius() const;
    float getMinorRadius() const;
    int getNumMajor() const;
    int getNumMinor() const;

private:
    void CreateTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments);
    
    float m_majorRadius;
    float m_minorRadius;
    int m_numMajor;
    int m_numMinor;

    std::vector<float> m_vertices;
    std::vector<unsigned int> m_indices;
};