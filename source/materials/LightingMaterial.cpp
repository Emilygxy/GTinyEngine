#include "materials/LightingMaterial.h"
#include "filesystem.h"

namespace te
{
    LightingMaterial::LightingMaterial()
        : MaterialBase(FileSystem::getPath("resources/shaders/deferred/lighting.vs").c_str(),
                      FileSystem::getPath("resources/shaders/deferred/lighting.fs").c_str())
        , mPositionTexture(0)
        , mNormalTexture(0)
        , mAlbedoTexture(0)
        , mLightPos(0.0f)
        , mLightColor(1.0f)
        , mLightIntensity(1.0f)
        , mViewPos(0.0f)
    {
    }

    LightingMaterial::~LightingMaterial()
    {
    }

    void LightingMaterial::SetGBufferTextures(GLuint position, GLuint normal, GLuint albedo)
    {
        mPositionTexture = position;
        mNormalTexture = normal;
        mAlbedoTexture = albedo;
    }

    void LightingMaterial::SetLightParameters(const glm::vec3& lightPos, const glm::vec3& lightColor, float intensity)
    {
        mLightPos = lightPos;
        mLightColor = lightColor;
        mLightIntensity = intensity;
    }

    void LightingMaterial::SetViewPosition(const glm::vec3& viewPos)
    {
        mViewPos = viewPos;
    }

    void LightingMaterial::OnBind()
    {
        // 绑定G-Buffer纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mPositionTexture);
        mpShader->setInt("gPosition", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mNormalTexture);
        mpShader->setInt("gNormal", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mAlbedoTexture);
        mpShader->setInt("gAlbedo", 2);
    }

    void LightingMaterial::OnPerFrameUpdate()
    {
        // 光照Pass不需要每帧更新
    }

    void LightingMaterial::UpdateUniform()
    {
        if (mpShader)
        {
            mpShader->setVec3("viewPos", mViewPos);
            mpShader->setVec3("lightPos", mLightPos);
            mpShader->setVec3("lightColor", mLightColor);
            mpShader->setFloat("lightIntensity", mLightIntensity);
        }
    }
}
