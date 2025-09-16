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
    // 创建天空盒Pass
    auto skyboxPass = std::make_shared<te::SkyboxPass>();
    skyboxPass->Initialize(te::RenderPassConfig{
        "SkyboxPass",
        te::RenderPassType::Skybox,
        te::RenderPassState::Enabled,
        {}, // inputs
        {
             {"BackgroundColor", "backgroundcolor", te::RenderTargetFormat::RGBA8},
        }, // outputs
        {}, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // enable deptyh test,LEQUAL
        }, g_RenderView, g_RenderContext);

    // 创建几何Pass
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(te::RenderPassConfig{
        "GeometryPass",
        te::RenderPassType::Geometry,
        te::RenderPassState::Enabled,
        {}, // inputs
        {
            {"Albedo", "albedo", te::RenderTargetFormat::RGBA8},
            {"Normal", "normal", te::RenderTargetFormat::RGB16F},
            {"Position", "position", te::RenderTargetFormat::RGB16F},
            {"Depth", "depth", te::RenderTargetFormat::Depth24}
        }, // outputs
        {}, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
    }, g_RenderView, g_RenderContext);

    // 创建光照Pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(te::RenderPassConfig{
        "BasePass",
        te::RenderPassType::Base,
        te::RenderPassState::Enabled,
        {
            {"Albedo", "GeometryPass", "albedo", 0, true},
            {"Normal", "GeometryPass", "normal", 0, true},
            {"Position", "GeometryPass", "position", 0, true},
            {"Depth", "GeometryPass", "depth", 0, true}
        }, // inputs
        {
            {"BaseColor", "basecolor", te::RenderTargetFormat::RGBA8}
        }, // outputs
        {
            {"GeometryPass", true, []() { return true; }},
        }, // dependencies
        true, true, false, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)  // enable depth test, clear color is (0,0,0,0) for combine with background
    }, g_RenderView, g_RenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(te::RenderPassConfig{
        "PostProcessPass",
        te::RenderPassType::PostProcess,
        te::RenderPassState::Enabled,
        {
            {"BackgroundColor", "SkyboxPass", "backgroundcolor", 0, true},
            {"BaseColor", "BasePass", "basecolor", 0, true}
        }, // inputs - BasePass now handles background fusion
        {}, // outputs - render to screen directly
        {
            {"SkyboxPass", true, []() { return true; }},
            {"BasePass", true, []() { return true; }}
        }, // dependencies
        true, false, false, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)  // 启用颜色清除
    }, g_RenderView, g_RenderContext);

    postProcessPass->AddEffect("Blit",std::make_shared<BlitMaterial>());

    // 添加Pass到渲染器
    g_renderer->AddRenderPass(geometryPass);
    g_renderer->AddRenderPass(basePass);
    g_renderer->AddRenderPass(postProcessPass);
    g_renderer->AddRenderPass(skyboxPass);
    
    // 添加Pass到RenderPassManager（用于依赖关系管理）
    te::RenderPassManager::GetInstance().AddPass(skyboxPass);
    te::RenderPassManager::GetInstance().AddPass(geometryPass);
    te::RenderPassManager::GetInstance().AddPass(basePass);
    te::RenderPassManager::GetInstance().AddPass(postProcessPass);

    // 启用多Pass渲染
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

    // 创建渲染器
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

    // 创建RenderView和RenderContext
    g_RenderView = std::make_shared<RenderView>(SCR_WIDTH, SCR_HEIGHT);
    g_RenderContext = std::make_shared<RenderContext>();
    g_renderer->SetRenderContext(g_RenderContext);

    // 创建相机
    g_camera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    g_camera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
    //g_renderer->SetCamera(g_camera);
    g_RenderContext->AttachCamera(g_camera);

    // 创建光源
    if (!g_light)
    {
        g_light = std::make_shared<Light>();
    }
    g_light = std::make_shared<Light>();
    g_light->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
    g_light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    //g_renderer->SetLight(g_light);
    g_RenderContext->PushAttachLight(g_light);

    // 创建几何体
    if (!g_sphere)
    {
        g_sphere = std::make_shared<Sphere>();
    }
    auto material = std::make_shared<BlinnPhongMaterial>();
    material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    material->AttachedLight(g_light);
    //material->AttachedLight(g_light);
    g_sphere->SetMaterial(material);

    // 设置多Pass渲染
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

        // 执行多Pass渲染
        if (g_renderer->IsMultiPassEnabled())
        {
            // 创建渲染命令
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
            
            // 使用RenderPassManager执行Pass（有正确的依赖关系管理）
            te::RenderPassManager::GetInstance().ExecuteAll(commands);
            //g_renderer->ExecuteRenderPasses(commands);
        }
        else
        {
            // 传统单Pass渲染
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
