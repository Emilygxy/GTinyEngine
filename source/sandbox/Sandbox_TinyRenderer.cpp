#include "sandbox/Sandbox_TinyRenderer.h"

#include "geometry/Sphere.h"
#include "materials/PBRMaterial.h"

#include <glm/gtc/matrix_transform.hpp>

void Sandbox_TinyRenderer::Init(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;

    mpGeometry = std::make_shared<Sphere>();

    auto material = std::make_shared<PBRMaterial>();
    material->SetAlbedoTexturePath("resources/textures/IMG_8516.JPG");

    mpGeometry->SetMaterial(material);
    mpGeometry->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.0f, -2.0f)));
}

void Sandbox_TinyRenderer::Update(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
}

void Sandbox_TinyRenderer::Teardown(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
    mpGeometry.reset();
}

std::shared_ptr<FragmentsSource> Sandbox_TinyRenderer::GetFragmentsSource() const
{
    return mpGeometry;
}
