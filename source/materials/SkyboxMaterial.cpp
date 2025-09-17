#include "materials/SkyboxMaterial.h"
#include "ultis.h"
#include "filesystem.h"
#include "Camera.h"

SkyboxMaterial::SkyboxMaterial(const std::string& vs_path, const std::string& fs_path)
    : MaterialBase(vs_path, fs_path)
{
    std::vector<std::string> faces
    {
        "resources/textures/skybox/right.jpg",
        "resources/textures/skybox/left.jpg",
        "resources/textures/skybox/top.jpg",
        "resources/textures/skybox/bottom.jpg",
        "resources/textures/skybox/front.jpg",
        "resources/textures/skybox/back.jpg"
    };
    mCubemapTexture = loadCubemap(faces);
}
SkyboxMaterial::~SkyboxMaterial()
{
    glDeleteTextures(1, &mCubemapTexture);
}

void SkyboxMaterial::OnBind()
{
    // bind cubemap texture to texture unit 7 to avoid conflicts with other passes
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mCubemapTexture);
}

void SkyboxMaterial::UnBind()
{
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void SkyboxMaterial::OnPerFrameUpdate()
{
    //todo
}

void SkyboxMaterial::UpdateUniform()
{
    // Set texture parameters - use texture unit 7
    mpShader->setInt("u_skyboxMap", 7); // Background texture
}

