#include "materials/PBRMaterial.h"
#include "Camera.h"
#include "Light.h"

PBRMaterial::PBRMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
    , mpAlbedoTexture(std::shared_ptr<Texture2D>())
{
}

PBRMaterial::~PBRMaterial()
{
}

void PBRMaterial::UpdateUniform()
{
    // invoke parent's UpdateUniform
    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());

        mpShader->setVec3("u_camPos", pCamera->GetEye());
    }
    // Set lighting parameters
    if (auto pLight = mpAttachedLight.lock())
    {
        mpShader->setVec3("u_lightColor", pLight->GetColor());
        mpShader->setVec3("u_lightPos", pLight->GetPosition());
    }

    mpShader->setVec3("u_albedo", mAlbedoColor);
    mpShader->setVec3("u_metallic_roughness_ao", mMetallicRoughnessAO);
}

void PBRMaterial::OnPerFrameUpdate()
{
}
void PBRMaterial::OnBind()
{
    // bind texture
    if (mpAlbedoTexture && mpAlbedoTexture->IsValid()) {
        if (mpAlbedoTexture->GetHandle() == 0) {
            std::cout << "ERROR: PBRMaterial::OnBind() - mpAlbedoTexture is 0!" << std::endl;
            return;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpAlbedoTexture->GetHandle());

        GLint boundTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
        if (boundTexture != mpAlbedoTexture->GetHandle()) {
            std::cout << "ERROR: PBRMaterial::OnBind() - Failed to bind texture! Expected: " << mpAlbedoTexture->GetHandle() << ", Got: " << boundTexture << std::endl;
        }
        else {
            std::cout << "PBRMaterial::OnBind() - Successfully bound texture " << mpAlbedoTexture->GetHandle() << " to GL_TEXTURE0" << std::endl;
        }
    }
}
void PBRMaterial::UnBind()
{
    MaterialBase::UnBind();

    //
}

void PBRMaterial::SetAlbedoColor(const glm::vec3& color)
{
    mAlbedoColor = color;
}
void PBRMaterial::SetMetallic(float val)
{
    mMetallicRoughnessAO.x = val;
}
void PBRMaterial::SetRoughness(float val)
{
    mMetallicRoughnessAO.y = val;
}
void PBRMaterial::SetAO(float val)
{
    mMetallicRoughnessAO.z = val;

}

void PBRMaterial::SetAlbedoTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PhongMaterial::SetDiffuseTexturePath - Empty path" << std::endl;
        return;
    }

    mpAlbedoTexture->SetTexturePaths({ path });
}