#include "framework/FullscreenQuad.h"
#include "materials/BlitMaterial.h"

namespace te
{
    FullscreenQuad::FullscreenQuad()
    {
        CreateFullscreenQuad();

        //material
        auto pbgMaterial = std::make_shared<BackgroundMaterial>();
        pbgMaterial->SetTexturePath("resources/textures/IMG_8515.JPG");
        SetMaterial(pbgMaterial);
    }

    void FullscreenQuad::CreateFullscreenQuad()
    {
        mVertices.clear();
        mIndices.clear();

        Vertex quadVertices[6] = {
            // positions   // texCoords
            Vertex(glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f)),
            Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f)),
            Vertex(glm::vec3( 1.0f, -1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f)),

            Vertex(glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f)),
            Vertex(glm::vec3( 1.0f, -1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f)),
            Vertex(glm::vec3( 1.0f,  1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f))
        };

        // create vertices data
        // front (0, 1, 2, 3)
        for (int i = 0; i < 6; ++i) {
            mVertices.push_back(quadVertices[i]);
            mIndices.push_back(i);
        }

        // set UV flag
        MarkHasUV(true);

        // set mesh
        SetupMesh();
    }

}
