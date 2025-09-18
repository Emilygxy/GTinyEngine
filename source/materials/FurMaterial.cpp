#include "materials/FurMaterial.h"
#include "Camera.h"
#include "Light.h"
#include <iostream>

FurMaterial::FurMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
{
    std::cout << "FurMaterial created with shaders: " << vs_path << ", " << fs_path << std::endl;
}

void FurMaterial::OnBind()
{
    if (!mpShader) {
        std::cout << "FurMaterial::OnBind() - No shader available!" << std::endl;
        return;
    }
    
    // bind fur texture (if any)
    // here can add fur texture binding logic
    
    std::cout << "FurMaterial::OnBind() - Shader bound successfully" << std::endl;
}

void FurMaterial::UpdateUniform()
{
    if (!mpShader) {
        std::cout << "FurMaterial::UpdateUniform() - No shader available!" << std::endl;
        return;
    }
    
    // set fur attribute uniforms
    mpShader->setFloat("u_hairLength", mHairLength);
    mpShader->setFloat("u_hairDensity", mHairDensity);
    mpShader->setVec3("u_hairColor", mHairColor);
    mpShader->setInt("u_numLayers", mNumLayers);
    mpShader->setFloat("u_currentLayer", mCurrentLayer);
    
    // set camera and light parameters
    if (auto pCamera = mpAttachedCamera.lock()) {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
        mpShader->setVec3("u_viewPos", pCamera->GetEye());
    }
    
    if (auto pLight = mpAttachedLight.lock()) {
        mpShader->setVec3("u_lightPos", pLight->GetPosition());
        mpShader->setVec3("u_lightColor", pLight->GetColor());
    }
    
    std::cout << "FurMaterial::UpdateUniform() - Uniforms updated" << std::endl;
}
