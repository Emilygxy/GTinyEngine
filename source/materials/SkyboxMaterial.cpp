#include "materials/SkyboxMaterial.h"
#include "ultis.h"
#include "filesystem.h"
#include "Camera.h"

SkyboxMaterial::SkyboxMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
{
    mpCubemapTexture = std::make_shared<TextureCube>();
    std::vector<std::string> faces
    {
        "resources/textures/skybox/right.jpg",
        "resources/textures/skybox/left.jpg",
        "resources/textures/skybox/top.jpg",
        "resources/textures/skybox/bottom.jpg",
        "resources/textures/skybox/front.jpg",
        "resources/textures/skybox/back.jpg"
    };
    SetDiffuseTexturePath(faces);
}

void SkyboxMaterial::SetDiffuseTexturePath(const std::vector<std::string>& paths)
{
    if (paths.size() < 6)
    {
        std::cout << "SkyboxMaterial faile to created cubemap texture with path number: " << int(paths.size()) << std::endl;
        return;
    }

    mpCubemapTexture->SetTexturePaths(paths);
}
SkyboxMaterial::~SkyboxMaterial()
{
    mpCubemapTexture->Destroy();
}

void SkyboxMaterial::OnBind()
{
    // Debug: Check texture validity before binding
    if (mpCubemapTexture->GetHandle() == 0) {
        std::cout << "ERROR: SkyboxMaterial::OnBind() - mCubemapTexture is 0!" << std::endl;
        return;
    }
    
    // bind cubemap texture to texture unit 0 to avoid conflicts with other passes
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mpCubemapTexture->GetHandle());
    
    // Debug: Verify binding
    GLint boundTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &boundTexture);
    if (boundTexture != mpCubemapTexture->GetHandle()) {
        std::cout << "ERROR: SkyboxMaterial::OnBind() - Failed to bind texture! Expected: " << mpCubemapTexture->GetHandle() << ", Got: " << boundTexture << std::endl;
    } else {
        std::cout << "SkyboxMaterial::OnBind() - Successfully bound cubemap texture " << mpCubemapTexture->GetHandle() << " to GL_TEXTURE0" << std::endl;
    }
}

void SkyboxMaterial::UnBind()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void SkyboxMaterial::OnPerFrameUpdate()
{
    //todo
}

void SkyboxMaterial::UpdateUniform()
{
    // Set texture parameters - use texture unit 0
    mpShader->setInt("u_skyboxMap", 0); // Background texture
    
    // Debug: Check if uniform was set correctly
    GLint uniformLocation = glGetUniformLocation(mpShader->GetID(), "u_skyboxMap");
    if (uniformLocation == -1) {
        std::cout << "ERROR: SkyboxMaterial::UpdateUniform() - u_skyboxMap uniform not found in shader!" << std::endl;
    } else {
        std::cout << "SkyboxMaterial::UpdateUniform() - Set u_skyboxMap to texture unit 0" << std::endl;
    }
}

