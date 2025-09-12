#include "geometry/Torus.h"

Torus::Torus(float majorRadius, float minorRadius, int majorSegments, int minorSegments)
    : BasicGeometry()
    , m_majorRadius(majorRadius)
    , m_minorRadius(minorRadius)
    , m_numMajor(majorSegments)
    , m_numMinor(minorSegments)
{
    CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments);

    //material
    auto pUnlitMaterial = std::make_shared<PhongMaterial>();
    pUnlitMaterial->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    SetMaterial(pUnlitMaterial);
}

Torus::~Torus()
{
}

void Torus::setMajorRadius(float majorRadius)
{
    m_majorRadius = majorRadius;
}

void Torus::setMinorRadius(float minorRadius)
{
    m_minorRadius = minorRadius;
}

void Torus::setNumMajor(int numMajor)
{
    m_numMajor = numMajor;
}

void Torus::setNumMinor(int numMinor)
{
    m_numMinor = numMinor;
}

float Torus::getMajorRadius() const
{
    return m_majorRadius;
}

float Torus::getMinorRadius() const
{
    return m_minorRadius;
}

int Torus::getNumMajor() const
{
    return m_numMajor;
}

int Torus::getNumMinor() const
{
    return m_numMinor;
}

void Torus::CreateTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments)
{
    mVertices.clear();
    mIndices.clear();
    float kpi = (float)M_PI;

    // generate vert
    for (int i = 0; i <= majorSegments; ++i) {
        float majorAngle = 2.0f * kpi * i / majorSegments;
        float cosMajor = cosf(majorAngle);
        float sinMajor = sinf(majorAngle);

        for (int j = 0; j <= minorSegments; ++j) {
            float minorAngle = 2.0f * kpi * j / minorSegments;
            float cosMinor = cosf(minorAngle);
            float sinMinor = sinf(minorAngle);

            // pos
            float x = (majorRadius + minorRadius * cosMinor) * cosMajor;
            float y = (majorRadius + minorRadius * cosMinor) * sinMajor;
            float z = minorRadius * sinMinor;

            // normal
            float nx = cosMinor * cosMajor;
            float ny = cosMinor * sinMajor;
            float nz = sinMinor;

            // uv
            float u = (float)i / majorSegments;
            float v = (float)j / minorSegments;

            Vertex vertex;
            vertex.position = glm::vec3(x, y, z);
            vertex.normal = glm::normalize(glm::vec3(nx, ny, nz));
            vertex.texCoords = glm::vec2(u, v);

            mVertices.push_back(vertex);
        }
    }

    // generate index
    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            int current = i * (minorSegments + 1) + j;
            int next = current + minorSegments + 1;

            // first tri
            mIndices.push_back(current);
            mIndices.push_back(next);
            mIndices.push_back(current + 1);

            // second tri
            mIndices.push_back(next);
            mIndices.push_back(next + 1);
            mIndices.push_back(current + 1);
        }
    }

    SetupMesh();
}