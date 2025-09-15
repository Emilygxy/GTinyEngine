#pragma once

#include "materials/BaseMaterial.h"

namespace te
{
    class LightingMaterial : public MaterialBase
    {
    public:
        LightingMaterial();
        ~LightingMaterial();

        void SetGBufferTextures(GLuint position, GLuint normal, GLuint albedo);
        void SetLightParameters(const glm::vec3& lightPos, const glm::vec3& lightColor, float intensity);
        void SetViewPosition(const glm::vec3& viewPos);

        void OnBind() override;
        void OnPerFrameUpdate() override;
        void UpdateUniform() override;

    private:
        GLuint mPositionTexture;
        GLuint mNormalTexture;
        GLuint mAlbedoTexture;
        
        glm::vec3 mLightPos;
        glm::vec3 mLightColor;
        float mLightIntensity;
        glm::vec3 mViewPos;
    };
}
