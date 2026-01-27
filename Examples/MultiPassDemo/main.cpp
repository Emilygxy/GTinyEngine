#include <iostream>
#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// GTinyEngine includes
#include "framework/Renderer.h"
#include "framework/RenderPass.h"
#include "framework/FrameBuffer.h"
#include "Camera.h"
#include "Light.h"
#include "geometry/Sphere.h"
#include "materials/BlinnPhongMaterial.h"
#include "materials/BlitMaterial.h"
#include "RenderView.h"
#include "framework/RenderContext.h"

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Global variables
std::shared_ptr<IRenderer> g_renderer = nullptr;
std::shared_ptr<RenderContext> g_RenderContext = nullptr;
std::shared_ptr<RenderView> g_RenderView = nullptr;
std::shared_ptr<Camera> g_camera = nullptr;
std::shared_ptr<Light> g_light = nullptr;
std::shared_ptr<Sphere> g_sphere = nullptr;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void setupMultiPassRendering()
{
    // Create skybox Pass
    auto skyboxPass = std::make_shared<te::SkyboxPass>();
    skyboxPass->Initialize(g_RenderView, g_RenderContext);

    // Create geometry Pass
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(g_RenderView, g_RenderContext);

    // Create lighting Pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(g_RenderView, g_RenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(g_RenderView, g_RenderContext);

    postProcessPass->AddEffect("Blit",std::make_shared<BlitMaterial>());

    // Add Pass to RenderPassManager (for dependency management)
    te::RenderPassManager::GetInstance().AddPass(skyboxPass);
    te::RenderPassManager::GetInstance().AddPass(geometryPass);
    te::RenderPassManager::GetInstance().AddPass(basePass);
    te::RenderPassManager::GetInstance().AddPass(postProcessPass);

    // Enable multi-pass rendering
    g_renderer->SetMultiPassEnabled(true);
}

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Multi-Pass Rendering Demo", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Create renderer
    std::cout << "Creating renderer..." << std::endl;
    g_renderer = RendererFactory::CreateRenderer(RendererBackend::OpenGL);
    if (!g_renderer)
    {
        std::cout << "Failed to create renderer" << std::endl;
        return -1;
    }
    
    std::cout << "Initializing renderer..." << std::endl;
    if (!g_renderer->Initialize())
    {
        std::cout << "Failed to initialize renderer" << std::endl;
        return -1;
    }
    
    std::cout << "Renderer created and initialized successfully" << std::endl;

    // Create RenderView and RenderContext
    g_RenderView = std::make_shared<RenderView>(SCR_WIDTH, SCR_HEIGHT);
    g_RenderContext = std::make_shared<RenderContext>();
    g_renderer->SetRenderContext(g_RenderContext);

    // Create camera
    g_camera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    g_camera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
    g_RenderContext->AttachCamera(g_camera);

    // Create light
    if (!g_light)
    {
        g_light = std::make_shared<Light>();
    }
    g_light = std::make_shared<Light>();
    g_light->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
    g_light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    g_RenderContext->PushAttachLight(g_light);

    // Create geometry
    if (!g_sphere)
    {
        g_sphere = std::make_shared<Sphere>();
    }
    auto material = std::make_shared<BlinnPhongMaterial>();
    material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    g_sphere->SetMaterial(material);

    // Setup multi-pass rendering
    setupMultiPassRendering();

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // input
        processInput(window);

        // Begin Render Frame
        g_renderer->BeginFrame();

        g_renderer->SetViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        g_renderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        g_renderer->Clear(0x3);

        // Execute multi-pass rendering
        if (g_renderer->IsMultiPassEnabled())
        {
            // Create render commands
            std::vector<RenderCommand> commands;
            RenderCommand sphereCommand;
            sphereCommand.material = g_sphere->GetMaterial();
            sphereCommand.vertices = g_sphere->GetVertices();
            sphereCommand.indices = g_sphere->GetIndices();
            sphereCommand.transform = g_sphere->GetWorldTransform();
            sphereCommand.state = RenderMode::Opaque;
            sphereCommand.hasUV = true;
            sphereCommand.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;

            commands.push_back(sphereCommand);
            
            // Use RenderPassManager to execute Pass (with correct dependency management)
            te::RenderPassManager::GetInstance().ExecuteAll(commands);
            //g_renderer->ExecuteRenderPasses(commands);
        }
        else
        {
            // Traditional single-pass rendering
            //g_renderer->DrawBackgroud();
            g_renderer->DrawMesh(g_sphere->GetVertices(),
                               g_sphere->GetIndices(),
                               g_sphere->GetMaterial(),
                               g_sphere->GetWorldTransform());
        }

        // End Render Frame
        g_renderer->EndFrame();

        // glfw: swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    g_renderer->Shutdown();
    glfwTerminate();
    return 0;
}
