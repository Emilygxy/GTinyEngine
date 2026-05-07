#include "HybridGSIntegration.h"

#include "framework/Renderer.h"
#include "framework/RenderContext.h"
#include "GaussianSplat/GSSceneLoader.h"
#include "GTVulkan/GlfwGeneral.h"
#include "mesh/Mesh.h"
#include "Camera.h"
#include "Light.h"

#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<RenderCommand> CreateSceneCommands()
{
    auto meshA = std::make_shared<Mesh>();
    auto meshB = std::make_shared<Mesh>();
    const Vertex vertices[] = {
        {{-0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{0.7f, -0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{0.7f, 0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.7f, 0.7f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };
    const int indices[] = {0, 1, 2, 0, 2, 3};
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
    return {cmdA, cmdB};
}

std::string ResolveDefaultPly(int argc, char** argv)
{
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        return std::string(argv[1]);
    }
    const std::string rel = "Examples/VK_HybridGSRenderDemo/assets/cloudpoints/sample_robot.ply";
    const std::string alt = "../../../Examples/VK_HybridGSRenderDemo/assets/cloudpoints/sample_robot.ply";
    if (std::filesystem::exists(rel)) {
        return rel;
    }
    if (std::filesystem::exists(alt)) {
        return alt;
    }
    return {};
}

} // namespace

int main(int argc, char** argv)
{
    const std::string plyPath = ResolveDefaultPly(argc, argv);
    if (plyPath.empty()) {
        std::cerr << "No PLY path and default assets not found." << std::endl;
        return -1;
    }

    GSSceneLoader loader;
    std::vector<GSVertex> gsVertices = loader.load(plyPath);
    if (gsVertices.empty()) {
        std::cerr << "Failed to load PLY: " << plyPath << std::endl;
        return -1;
    }

    auto rendererBase = RendererFactory::CreateRenderer(RendererBackend::Vulkan);
    auto vkRenderer = std::dynamic_pointer_cast<VulkanRenderer>(rendererBase);
    if (!vkRenderer) {
        std::cerr << "Vulkan renderer unavailable." << std::endl;
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
    vkRenderer->SetRenderContext(renderContext);

    HybridGSIntegration hybrid(*vkRenderer, std::move(gsVertices));
    vkRenderer->SetHybridPreprocessCallback([&hybrid]() { hybrid.onPreprocess(); });
    vkRenderer->SetHybridAfterLightingCallback(
        [&hybrid](VkCommandBuffer cmd, uint32_t imageIndex) { hybrid.onAfterLighting(cmd, imageIndex); });

    if (!vkRenderer->Initialize()) {
        std::cerr << "VulkanRenderer::Initialize failed." << std::endl;
        return -1;
    }
    if (!hybrid.attachCamera(camera)) {
        std::cerr << "GSComputeSubsystem::initialize failed." << std::endl;
        vkRenderer->Shutdown();
        return -1;
    }
    if (!hybrid.createCompositorAfterRendererInit()) {
        std::cerr << "Compositor pipeline creation failed." << std::endl;
        vkRenderer->Shutdown();
        return -1;
    }

    std::vector<RenderCommand> commands = CreateSceneCommands();
    while (!glfwWindowShouldClose(easy_vk::pWindow)) {
        while (glfwGetWindowAttrib(easy_vk::pWindow, GLFW_ICONIFIED)) {
            glfwWaitEvents();
        }

        const float t = static_cast<float>(glfwGetTime());
        for (size_t i = 0; i < commands.size(); ++i) {
            auto mesh = std::dynamic_pointer_cast<Mesh>(commands[i].fragmentsSource);
            if (!mesh) {
                continue;
            }
            const float baseX = (i == 0) ? -0.6f : 0.6f;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(baseX, 0.0f, 0.0f));
            model = glm::rotate(model, t * (i == 0 ? 1.0f : -1.3f), glm::vec3(0.0f, 0.0f, 1.0f));
            mesh->SetWorldTransform(model);
        }

        vkRenderer->BeginFrame();
        vkRenderer->DrawMeshes(commands);
        vkRenderer->EndFrame();

        glfwPollEvents();
        easy_vk::TitleFps();
    }

    hybrid.shutdownGpu();
    vkRenderer->Shutdown();
    return 0;
}
