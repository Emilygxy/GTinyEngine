#include "materials/BaseMaterial.h"
#include <iostream>
#include "ultis.h"
#include "glad/glad.h"
#include "Camera.h"
#include "Light.h"
#include "filesystem.h"

MaterialBase::MaterialBase(const std::string& vs_path, const std::string& fs_path)
{
	mpShader = std::make_shared<Shader>(vs_path.c_str(), fs_path.c_str());
}

void MaterialBase::AttachedCamera(const std::shared_ptr<Camera>& pcamera)
{
    mpAttachedCamera = pcamera;
}

void MaterialBase::AttachedLight(const std::shared_ptr<Light>& pLight)
{
    mpAttachedLight = pLight;
}

void MaterialBase::OnApply()
{
    mpShader->use();
}

UnlitMaterial::UnlitMaterial(const std::string& vs_path, const std::string& fs_path)
	: MaterialBase(vs_path, fs_path)
    , mpTexture(std::make_shared<Texture2D>())
{

}
UnlitMaterial::~UnlitMaterial()
{
}

void UnlitMaterial::OnBind()
{
    // bind texture
    if (mbHasTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpTexture->GetHandle());
    }
}

void UnlitMaterial::OnPerFrameUpdate()
{
    //todo
}

void UnlitMaterial::UpdateUniform()
{
    // Note: model matrix is set by the renderer, don't override it here
    
    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
    }

    // Set lighting parameters
    mpShader->setVec3("objectColor", glm::vec3(0.7f, 0.3f, 0.3f));
    mpShader->setInt("diffuseTexture", 0); //uniform location?
}

void UnlitMaterial::SetTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "UnlitMaterial::SetTexturePath - Empty path" << std::endl;
        return;
    }

    mpTexture->SetTexturePaths({ path });
    mbHasTexture = true;
}

PhongMaterial::PhongMaterial(const std::string& vs_path, const std::string& fs_path)
	: MaterialBase(vs_path, fs_path)
    , mpDiffuseTexture(std::make_shared<Texture2D>())
{
    mUseEnables = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

PhongMaterial::~PhongMaterial()
{
}

void PhongMaterial::OnBind()
{
    // bind texture
    if (mbHasTexture) {
        if (mpDiffuseTexture->GetHandle() == 0) {
            std::cout << "ERROR: PhongMaterial::OnBind() - mpDiffuseTexture is 0!" << std::endl;
            return;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mpDiffuseTexture->GetHandle());

        GLint boundTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
        if (boundTexture != mpDiffuseTexture->GetHandle()) {
            std::cout << "ERROR: PhongMaterial::OnBind() - Failed to bind texture! Expected: " << mpDiffuseTexture->GetHandle() << ", Got: " << boundTexture << std::endl;
        }
        else {
            std::cout << "PhongMaterial::OnBind() - Successfully bound texture " << mpDiffuseTexture->GetHandle() << " to GL_TEXTURE0" << std::endl;
        }
    }
}

void PhongMaterial::UnBind()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PhongMaterial::UpdateUniform()
{
    // Note: model matrix is set by the renderer, don't override it here

    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
        mpShader->setVec3("u_viewPos", pCamera->GetEye());
    }
    
    mpShader->setVec3("u_objectColor", glm::vec3(0.7f, 0.3f, 0.3f));
    mpShader->setVec4("u_Strengths", mIntensities);
    
    mpShader->setInt("u_diffuseTexture", 0);
    mpShader->setInt("u_geomAlbedoMap", 1);
    mpShader->setInt("u_geomNormalMap", 2);
    mpShader->setInt("u_geomPositionMap", 3);
    mpShader->setInt("u_geomDepthMap", 4);

    // Set lighting parameters
    if (auto pLight = mpAttachedLight.lock())
    {
        mpShader->setVec3("u_lightColor", pLight->GetColor());
        mpShader->setVec3("u_lightPos", pLight->GetPosition());
    }

    mpShader->setVec4("u_useBlinn_Geometry", mUseEnables);
}

void PhongMaterial::SetDiffuseTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "PhongMaterial::SetDiffuseTexturePath - Empty path" << std::endl;
        return;
    }

    mpDiffuseTexture->SetTexturePaths({ path });
    mbHasTexture = true;
}

void PhongMaterial::SetUseGeometryTarget(bool use)
{
    mUseEnables[1] = use ? 1.0f : 0.0f;
}
