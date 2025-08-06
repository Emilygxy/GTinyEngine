#include "Renderer.h"

namespace te
{
    void Renderer::InitCamera(unsigned int SCR_WIDTH, unsigned int SCR_HEIGHT)
    {
        mpCamera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));

        mpCamera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
    }

    void Renderer::InitLight()
    {
        mpLight = std::make_shared<Light>();
    }

    void Renderer::InitSkybox()
    {
        std::vector<std::string> faces
        {
            FileSystem::getPath("resources/textures/skybox/right.jpg"),
            FileSystem::getPath("resources/textures/skybox/left.jpg"),
            FileSystem::getPath("resources/textures/skybox/top.jpg"),
            FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
            FileSystem::getPath("resources/textures/skybox/front.jpg"),
            FileSystem::getPath("resources/textures/skybox/back.jpg")
        };
        mpSkybox = std::make_shared<Skybox>(faces);
    }

    void Renderer::InitGeometry()
    {
        //g_pSphere = std::make_shared<Sphere>(1.0f, 32, 32);
        mpGeometry = std::make_shared<Torus>();
        mpGeometry->GetMaterial()->AttachedCamera(mpCamera);
        mpGeometry->GetMaterial()->AttachedLight(mpLight);
    }

    std::shared_ptr<Camera> Renderer::GetCamera() const noexcept
    {
        return mpCamera;
    }

    std::shared_ptr<Light> Renderer::GetLight() const noexcept
    {
        return mpLight;
    }

    std::shared_ptr<Skybox> Renderer::GetSkybox() const noexcept
    {
        return mpSkybox;
    }

    std::shared_ptr<BasicGeometry> Renderer::GetGeometry() const noexcept
    {
        return mpGeometry;
    }

    void Renderer::Render()
    {
        //render skybox first
        mpSkybox->Draw(mpCamera->GetViewMatrix(), mpCamera->GetProjectionMatrix());

        // apply shader
        mpGeometry->GetMaterial()->OnApply();
        mpGeometry->GetMaterial()->UpdateUniform();

        // Render sphere
        mpGeometry->Draw();
    }
}