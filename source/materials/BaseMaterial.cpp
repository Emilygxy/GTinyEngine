#include "materials/BaseMaterial.h"
#include <iostream>
#include "ultis.h"
#include <glm/glm.hpp>
#include "glad/glad.h"
#include "shader.h"
#include "Camera.h"

MaterialBase::MaterialBase(const std::string& vs_path, const std::string& fs_path)
{
	mpShader = std::make_shared<Shader>(vs_path.c_str(), fs_path.c_str());
}

void MaterialBase::AttachedCamera(const std::shared_ptr<Camera>& pcamera)
{
    mpAttachedCamera = pcamera;
}

void MaterialBase::OnApply()
{
    mpShader->use();
}

UnlitMaterial::UnlitMaterial(const std::string& vs_path, const std::string& fs_path)
	: MaterialBase(vs_path, fs_path)
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
        glBindTexture(GL_TEXTURE_2D, mTextureID);
    }
}

void UnlitMaterial::OnPerFrameUpdate()
{
    //todo
}

void UnlitMaterial::UpdateUniform()
{
    // Set transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
   /* model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.5f, 1.0f, 0.0f));*/

    mpShader->setMat4("model", model);

    if (auto pCamera = mpAttachedCamera.lock())
    {
        mpShader->setMat4("view", pCamera->GetViewMatrix());
        mpShader->setMat4("projection", pCamera->GetProjectionMatrix());
    }

    // Set lighting parameters
    mpShader->setVec3("objectColor", glm::vec3(0.7f, 0.3f, 0.3f));
    mpShader->setInt("diffuseTexture", 0);
}

void UnlitMaterial::SetTexturePath(const std::string& path)
{
    if (path.empty())
    {
        std::cout << "Sphere::SetTexturePath - Empty path" << std::endl;
        return;
    }

    mTextureID = loadTexture(path.c_str());
    mbHasTexture = true;
}

PhongMaterial::PhongMaterial(const std::string& vs_path, const std::string& fs_path)
	: MaterialBase(vs_path, fs_path)
{
}

PhongMaterial::~PhongMaterial()
{
}