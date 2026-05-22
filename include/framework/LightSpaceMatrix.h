#pragma once

#include <memory>
#include <glm/glm.hpp>
#include "mesh/AaBB.h"

class FragmentsSource;

namespace te
{
    struct LightSpaceMatrixParams
    {
        float orthoPadding = 2.0f;
        float shadowDistance = 20.0f;
        float nearPlane = 0.1f;
        float farPlane = 50.0f;
    };

    struct LightSpaceMatrices
    {
        glm::mat4 lightSpace{ 1.0f };
        glm::mat4 lightProjection{ 1.0f };
        glm::mat4 lightView{ 1.0f };
    };

    /// Builds orthographic light-space matrices for a directional light and scene AABB.
    LightSpaceMatrices ComputeDirectionalLightSpaceMatrix(
        const glm::vec3& lightDirection,
        const AaBB& sceneBounds,
        const LightSpaceMatrixParams& params = {});

    /// Union world AABBs from all fragments in a fragments source (Mesh / geometry).
    AaBB ComputeSceneBoundsFromFragmentsSource(const std::shared_ptr<FragmentsSource>& source);
}
