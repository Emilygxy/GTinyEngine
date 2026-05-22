#include "framework/LightSpaceMatrix.h"
#include "Fragment.h"
#include "mesh/Mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace te
{
    namespace
    {
        constexpr float kDefaultExtent = 5.0f;

        AaBB DefaultSceneBounds()
        {
            return AaBB(
                glm::vec3(-kDefaultExtent),
                glm::vec3(kDefaultExtent));
        }

        glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len = glm::length(v);
            if (len < 1e-5f)
            {
                return fallback;
            }
            return v / len;
        }
    }

    AaBB ComputeSceneBoundsFromFragmentsSource(const std::shared_ptr<FragmentsSource>& source)
    {
        AaBB bounds = AaBB::EmptyAaBB();
        if (!source)
        {
            return bounds;
        }

        if (auto mesh = std::dynamic_pointer_cast<Mesh>(source))
        {
            if (auto worldAabb = mesh->GetWorldAABB())
            {
                if (worldAabb->IsEmpty())
                {
                    return bounds;
                }
                return worldAabb.value();
            }
            return bounds;
        }

        const auto& fragments = source->GetFragments();
        for (const auto& frag : fragments)
        {
            if (!frag.mpGeometry)
            {
                continue;
            }
            if (auto worldAabb = frag.mpGeometry->GetWorldAABB())
            {
                if (!worldAabb->IsEmpty())
                {
                    bounds.Union(worldAabb.value());
                }
            }
        }
        return bounds;
    }

    LightSpaceMatrices ComputeDirectionalLightSpaceMatrix(
        const glm::vec3& lightDirection,
        const AaBB& sceneBounds,
        const LightSpaceMatrixParams& params)
    {
        AaBB bounds = sceneBounds.IsEmpty() ? DefaultSceneBounds() : sceneBounds;

        const glm::vec3 sceneCenter = (bounds.min + bounds.max) * 0.5f;
        const glm::vec3 lightDir = SafeNormalize(lightDirection, glm::vec3(0.0f, -1.0f, 0.0f));

        const glm::vec3 lightPos = sceneCenter - lightDir * params.shadowDistance;
        const glm::vec3 up = (std::abs(lightDir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        LightSpaceMatrices result;
        result.lightView = glm::lookAt(lightPos, sceneCenter, up);

        glm::vec3 lightSpaceMin(std::numeric_limits<float>::max());
        glm::vec3 lightSpaceMax(std::numeric_limits<float>::lowest());

        const glm::vec3 corners[8] = {
            { bounds.min.x, bounds.min.y, bounds.min.z },
            { bounds.max.x, bounds.min.y, bounds.min.z },
            { bounds.min.x, bounds.max.y, bounds.min.z },
            { bounds.max.x, bounds.max.y, bounds.min.z },
            { bounds.min.x, bounds.min.y, bounds.max.z },
            { bounds.max.x, bounds.min.y, bounds.max.z },
            { bounds.min.x, bounds.max.y, bounds.max.z },
            { bounds.max.x, bounds.max.y, bounds.max.z },
        };

        for (const glm::vec3& corner : corners)
        {
            const glm::vec4 lightViewCorner = result.lightView * glm::vec4(corner, 1.0f);
            lightSpaceMin = glm::min(lightSpaceMin, glm::vec3(lightViewCorner));
            lightSpaceMax = glm::max(lightSpaceMax, glm::vec3(lightViewCorner));
        }

        const float pad = params.orthoPadding;
        result.lightProjection = glm::ortho(
            lightSpaceMin.x - pad, lightSpaceMax.x + pad,
            lightSpaceMin.y - pad, lightSpaceMax.y + pad,
            params.nearPlane, params.farPlane);

        result.lightSpace = result.lightProjection * result.lightView;
        return result;
    }
}
