#include "sandbox/Sandbox_ShadowRenderingDemo.h"

#include "geometry/BasicGeometry.h"
#include "geometry/Sphere.h"
#include "materials/PBRMaterial.h"

#include <glm/gtc/matrix_transform.hpp>

void Sandbox_ShadowRenderingDemo::Init(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;

    mpGeometry = std::make_shared<Sphere>();
    mpPlaneGeometry = std::make_shared<Plane>(4.0f, 4.0f);

    auto material = std::make_shared<PBRMaterial>();
    material->SetAlbedoTexturePath("resources/textures/IMG_8516.JPG");
    mpGeometry->SetMaterial(material);
    mpGeometry->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.0f, -2.0f)));

    auto planeMaterial = std::make_shared<PBRMaterial>();
    //planeMaterial->SetAlbedoTexturePath("resources/textures/IMG_8516.JPG");
    mpPlaneGeometry->SetMaterial(planeMaterial);
    glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    planeTransform = glm::rotate(planeTransform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    planeTransform = glm::scale(planeTransform, glm::vec3(5.0f, 5.0f, 1.0f));
    mpPlaneGeometry->SetWorldTransform(planeTransform);
}

void Sandbox_ShadowRenderingDemo::Update(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
}

void Sandbox_ShadowRenderingDemo::Teardown(const std::shared_ptr<IRenderer>& renderer)
{
    (void)renderer;
    mpGeometry.reset();
    mpPlaneGeometry.reset();
}

std::shared_ptr<FragmentsSource> Sandbox_ShadowRenderingDemo::GetFragmentsSource() const
{
    return mpGeometry;
}

std::vector<RenderCommand> Sandbox_ShadowRenderingDemo::GetRenderCommands() const
{
    std::vector<RenderCommand> commands;
    commands.reserve(2);

    if (mpGeometry)
    {
        RenderCommand sphereCommand;
        sphereCommand.fragmentsSource = mpGeometry;
        sphereCommand.state = RenderMode::Opaque;
        sphereCommand.hasUV = true;
        sphereCommand.renderpassflag = RenderPassFlag::Shadowing
                                   | RenderPassFlag::Geometry
                                   | RenderPassFlag::BaseColor;
        commands.push_back(sphereCommand);
    }

    if (mpPlaneGeometry)
    {
        RenderCommand planeCommand;
        planeCommand.fragmentsSource = mpPlaneGeometry;
        planeCommand.state = RenderMode::Opaque;
        planeCommand.hasUV = true;
        planeCommand.renderpassflag = RenderPassFlag::Shadowing
                                  | RenderPassFlag::Geometry
                                  | RenderPassFlag::BaseColor;
        commands.push_back(planeCommand);
    }

    return commands;
}
