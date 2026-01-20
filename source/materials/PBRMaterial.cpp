#include "materials/PBRMaterial.h"
#include "Camera.h"
#include "Light.h"
#include "textures/Texture.h"
#include "glad/glad.h"
#include <iostream>

PBRMaterial::PBRMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
{
}

PBRMaterial::~PBRMaterial()
{
}

void PBRMaterial::OnBind()
{
    // Bind PBR textures to texture units
    // Texture unit 0: Albedo
    if (mpAlbedoTexture && mpAlbedoTexture->IsValid())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpAlbedoTexture->GetHandle());
    }
    
    // Texture unit 1: Normal
    if (mpNormalTexture && mpNormalTexture->IsValid())
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mpNormalTexture->GetHandle());
    }
    
    // Texture unit 2: Metallic
    if (mpMetallicTexture && mpMetallicTexture->IsValid())
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mpMetallicTexture->GetHandle());
    }
    
    // Texture unit 3: Roughness
    if (mpRoughnessTexture && mpRoughnessTexture->IsValid())
    {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mpRoughnessTexture->GetHandle());
    }
    
    // Texture unit 4: AO
    if (mpAOTexture && mpAOTexture->IsValid())
    {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, mpAOTexture->GetHandle());
    }
}

void PBRMaterial::UnBind()
{
    // Unbind all texture units
    for (int i = 0; i < 5; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void PBRMaterial::UpdateUniform()
{
    // Note: model matrix is set by the renderer, don't override it here

    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
        mpShader->setVec3("u_viewPos", pCamera->GetEye());
    }

    // Set material properties (fallback when textures are not available)
    mpShader->setVec3("u_albedo", mAlbedo);
    mpShader->setFloat("u_metallic", mMetallic);
    mpShader->setFloat("u_roughness", mRoughness);
    mpShader->setFloat("u_ao", mAO);

    // Set texture unit indices
    mpShader->setInt("u_albedoMap", 0);
    mpShader->setInt("u_normalMap", 1);
    mpShader->setInt("u_metallicMap", 2);
    mpShader->setInt("u_roughnessMap", 3);
    mpShader->setInt("u_aoMap", 4);

    // Set texture flags (which textures are available)
    glm::vec4 hasTextures(
        (mpAlbedoTexture && mpAlbedoTexture->IsValid()) ? 1.0f : 0.0f,
        (mpNormalTexture && mpNormalTexture->IsValid()) ? 1.0f : 0.0f,
        (mpMetallicTexture && mpMetallicTexture->IsValid()) ? 1.0f : 0.0f,
        (mpRoughnessTexture && mpRoughnessTexture->IsValid()) ? 1.0f : 0.0f
    );
    mpShader->setVec4("u_hasTextures", hasTextures);

    // Set lighting parameters
    if (auto pLight = mpAttachedLight.lock())
    {
        mpShader->setVec3("u_lightColor", pLight->GetColor());
        mpShader->setVec3("u_lightPos", pLight->GetPosition());
    }
    
    // Set brightness and lighting controls
    glm::vec2 intensities(mLightIntensity, mAmbientIntensity);
    mpShader->setVec2("u_intensities", intensities);
    mpShader->setFloat("u_exposure", mExposure);
    
    // Disable IBL by default (can be enabled later if needed)
    glm::vec2 use_ibl_ao(0.0f, (mpAOTexture && mpAOTexture->IsValid()) ? 1.0f : 0.0f);
    mpShader->setVec2("u_useIBL_ao", use_ibl_ao);
}
void PBRMaterial::SetAlbedoTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PBRMaterial::SetAlbedoTexturePath - Empty path" << std::endl;
        return;
    }
    if (!mpAlbedoTexture)
    {
        mpAlbedoTexture = std::make_shared<Texture2D>();
    }
    mpAlbedoTexture->SetTexturePaths({ path });
}

void PBRMaterial::SetNormalTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PBRMaterial::SetNormalTexturePath - Empty path" << std::endl;
        return;
    }
    if (!mpNormalTexture)
    {
        mpNormalTexture = std::make_shared<Texture2D>();
    }
    mpNormalTexture->SetTexturePaths({ path });
}

void PBRMaterial::SetRoughnessTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PBRMaterial::SetRoughnessTexturePath - Empty path" << std::endl;
        return;
    }
    if (!mpRoughnessTexture)
    {
        mpRoughnessTexture = std::make_shared<Texture2D>();
    }
    mpRoughnessTexture->SetTexturePaths({ path });
}

void PBRMaterial::SetMetallicTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PBRMaterial::SetMetallicTexturePath - Empty path" << std::endl;
        return;
    }
    if (!mpMetallicTexture)
    {
        mpMetallicTexture = std::make_shared<Texture2D>();
    }
    mpMetallicTexture->SetTexturePaths({ path });
}

void PBRMaterial::SetAOTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PBRMaterial::SetAOTexturePath - Empty path" << std::endl;
        return;
    }
    if (!mpAOTexture)
    {
        mpAOTexture = std::make_shared<Texture2D>();
    }
    mpAOTexture->SetTexturePaths({ path });
}
