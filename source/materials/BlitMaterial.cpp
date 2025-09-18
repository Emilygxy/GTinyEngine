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
