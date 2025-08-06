#pragma once

#include "Camera.h"
#include "Light.h"
#include "Skybox.h"
#include "filesystem.h"

#include "geometry/Sphere.h"
#include "geometry/Torus.h"
#include "materials/BaseMaterial.h"

class Renderer
{
public:
    Renderer() {}
    ~Renderer() {}

    void Render();

    void InitCamera(unsigned int SCR_WIDTH, unsigned int SCR_HEIGHT);
    void InitSkybox();
    void InitLight();
    void InitGeometry();

    std::shared_ptr<Camera> GetCamera() const noexcept;
    std::shared_ptr<Light> GetLight() const noexcept;
    std::shared_ptr<Skybox> GetSkybox() const noexcept;
    std::shared_ptr<BasicGeometry> GetGeometry() const noexcept;
private:
    std::shared_ptr<Camera> mpCamera = nullptr;
    std::shared_ptr<Light> mpLight = nullptr;
    std::shared_ptr<Skybox> mpSkybox = nullptr;

    std::shared_ptr<BasicGeometry> mpGeometry = nullptr;
};