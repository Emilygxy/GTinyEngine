#include "materials/BlitMaterial.h"
#include "Camera.h"

BlitMaterial::BlitMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
    , mpTexture(std::make_shared<Texture2D>())
{

}
BlitMaterial::~BlitMaterial()
{
}

void BlitMaterial::OnBind()
{
    // bind texture
    if (mbHasTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpTexture->GetHandle());
    }
}

void BlitMaterial::OnPerFrameUpdate()
{
    //todo
}

void BlitMaterial::UpdateUniform()
{
    // Note: model matrix is set by the renderer, don't override it here

    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
    }

    // Set texture parameters
    mpShader->setInt("u_backgroundMap", 0); // Background texture
    mpShader->setInt("u_screenTexture", 1); // BaseColor from BasePass
}

void BlitMaterial::SetTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "UnlitMaterial::SetTexturePath - Empty path" << std::endl;
        return;
    }

    mpTexture->SetTexturePaths({ path });
    mbHasTexture = true;
}

//=======BackgroundMaterial==========
BackgroundMaterial::BackgroundMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
    , mpTexture(std::make_shared<Texture2D>())
{

}
BackgroundMaterial::~BackgroundMaterial()
{
}

void BackgroundMaterial::OnBind()
{
    // bind texture
    if (mbHasTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpTexture->GetHandle());
    }
}

void BackgroundMaterial::OnPerFrameUpdate()
{
    //todo
}

void BackgroundMaterial::UpdateUniform()
{
    // Note: model matrix is set by the renderer, don't override it here

    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
    }

    // Set texture parameters
    mpShader->setInt("u_backgroundMap", 0); // Background texture
    glm::vec2 val = glm::vec2(0.0f); //  EnvironmentType::Color
    if (mEnvType == EnvironmentType::Image)
    {
        if (mbHasTexture)
        {
            val[0] = 1.0f;
            val[1] = 0.0f;
        }
    }
    else if(mEnvType == EnvironmentType::Hybrid)
    {
        val[0] = 1.0f;
        val[1] = 0.5f;
    }

    mpShader->setVec2("u_texture_factor", val);
    mpShader->setVec4("u_backgroungColor", mBackgroundColor);
}

void BackgroundMaterial::SetBackgroundColor(const glm::vec4& color)
{

}

void BackgroundMaterial::SetTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "BackgroundMaterial::SetTexturePath - Empty path" << std::endl;
        return;
    }

    mpTexture->SetTexturePaths({ path });
    mbHasTexture = true;
}

