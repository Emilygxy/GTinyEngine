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
}

void SkyboxMaterial::OnBind()
{
    // bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mCubemapTexture);
}

void SkyboxMaterial::OnPerFrameUpdate()
{
    //todo
}

void SkyboxMaterial::UpdateUniform()
{
    // Set texture parameters
    mpShader->setInt("u_skyboxMap", 0); // Background texture
}

