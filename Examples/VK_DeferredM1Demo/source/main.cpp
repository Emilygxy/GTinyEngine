#include "framework/Renderer.h"
#include "framework/RenderContext.h"
#include "GTVulkan/GlfwGeneral.h"
#include "mesh/Mesh.h"
#include "Camera.h"
#include "Light.h"

#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>

namespace {

std::vector<RenderCommand> CreateSceneCommands()
{
    auto meshA = std::make_shared<Mesh>();
    auto meshB = std::make_shared<Mesh>();
    const Vertex vertices[] = {
        { {-0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
        { { 0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
        { { 0.7f,  0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },
        { {-0.7f,  0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} }
    };
    const int indices[] = { 0, 1, 2, 0, 2, 3 };
    meshA->DoGenerateMesh(vertices, 4, indices, 6, true);
    meshB->DoGenerateMesh(vertices, 4, indices, 6, true);
    meshA->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-0.6f, 0.0f, 0.0f)));
    meshB->SetWorldTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.0f, 0.0f)));

    RenderCommand cmdA;
    cmdA.fragmentsSource = meshA;
    cmdA.hasUV = true;
    cmdA.state = RenderMode::Opaque;
    RenderCommand cmdB;
    cmdB.fragmentsSource = meshB;
    cmdB.hasUV = true;
    cmdB.state = RenderMode::Opaque;
    return { cmdA, cmdB };
}

} // namespace

int main()
{
    auto renderer = RendererFactory::CreateRenderer(RendererBackend::Vulkan);
    if (!renderer || !renderer->Initialize()) {
        return -1;
    }

    auto renderContext = std::make_shared<RenderContext>();
    auto camera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 2.0f));
    camera->SetAspectRatio(1280.0f / 720.0f);
    auto light = std::make_shared<Light>();
    light->SetPosition(glm::vec3(0.5f, 1.0f, 0.2f));
    light->SetColor(glm::vec3(1.0f, 0.95f, 0.9f));
    renderContext->AttachCamera(camera);
    renderContext->PushAttachLight(light);
    renderer->SetRenderContext(renderContext);

    std::vector<RenderCommand> commands = CreateSceneCommands();
    while (!glfwWindowShouldClose(easy_vk::pWindow)) {
        while (glfwGetWindowAttrib(easy_vk::pWindow, GLFW_ICONIFIED)) {
            glfwWaitEvents();
        }

        const float t = static_cast<float>(glfwGetTime());
        for (size_t i = 0; i < commands.size(); ++i) {
            auto mesh = std::dynamic_pointer_cast<Mesh>(commands[i].fragmentsSource);
            if (!mesh) continue;
            const float baseX = (i == 0) ? -0.6f : 0.6f;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(baseX, 0.0f, 0.0f));
            model = glm::rotate(model, t * (i == 0 ? 1.0f : -1.3f), glm::vec3(0.0f, 0.0f, 1.0f));
            mesh->SetWorldTransform(model);
        }

        renderer->BeginFrame();
        renderer->DrawMeshes(commands);
        renderer->EndFrame();

        glfwPollEvents();
        easy_vk::TitleFps();
    }

    renderer->Shutdown();
    return 0;
}

