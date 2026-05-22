#pragma once

#include "materials/BaseMaterial.h"
#include <glm/glm.hpp>

namespace te
{
    class ShadowDepthMaterial : public MaterialBase
    {
    public:
        ShadowDepthMaterial();
        ~ShadowDepthMaterial() override = default;

        void OnBind() override {}
        void OnPerFrameUpdate() override {}
        void UpdateUniform() override;

        void SetLightSpaceMatrix(const glm::mat4& matrix) { mLightSpaceMatrix = matrix; }
        const glm::mat4& GetLightSpaceMatrix() const { return mLightSpaceMatrix; }

    private:
        glm::mat4 mLightSpaceMatrix{ 1.0f };
    };
}
