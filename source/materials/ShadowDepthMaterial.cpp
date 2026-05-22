#include "materials/ShadowDepthMaterial.h"

namespace te
{
    ShadowDepthMaterial::ShadowDepthMaterial()
        : MaterialBase(
            "resources/shaders/shadow/shadow_depth.vs",
            "resources/shaders/shadow/shadow_depth.fs")
    {
    }

    void ShadowDepthMaterial::UpdateUniform()
    {
        if (!mpShader)
        {
            return;
        }
        mpShader->setMat4("lightSpaceMatrix", mLightSpaceMatrix);
    }
}
