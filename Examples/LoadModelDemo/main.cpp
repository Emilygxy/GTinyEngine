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
#include "Model.h"

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Global variables
std::shared_ptr<IRenderer> g_renderer = nullptr;
std::shared_ptr<RenderContext> g_RenderContext = nullptr;
std::shared_ptr<RenderView> g_RenderView = nullptr;
std::shared_ptr<Camera> g_camera = nullptr;
std::shared_ptr<Light> g_light = nullptr;
std::shared_ptr<Model> g_model = nullptr;

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
    skyboxPass->Initialize(g_RenderView, g_RenderContext);

    // 创建几何Pass
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(g_RenderView, g_RenderContext);

    // 创建光照Pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(g_RenderView, g_RenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(g_RenderView, g_RenderContext);

    postProcessPass->AddEffect("Blit",std::make_shared<BlitMaterial>());

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
    g_RenderContext->AttachCamera(g_camera);

    // 创建光源
    if (!g_light)
    {
        g_light = std::make_shared<Light>();
    }
    g_light = std::make_shared<Light>();
    g_light->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
    g_light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    g_RenderContext->PushAttachLight(g_light);

    // 创建几何体
    if (!g_model)
    {
        g_model = std::make_shared<Model>();
        g_model->loadModel("resources/models/rock/rock.obj");
    }
   /* auto material = std::make_shared<BlinnPhongMaterial>();
    material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    g_sphere->SetMaterial(material);*/

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
            for (const auto& mesh: g_model->GetMeshList())
            {
                RenderCommand meshCommand;
                meshCommand.material = mesh->GetMaterial();
                if (auto blinphong = std::dynamic_pointer_cast<BlinnPhongMaterial>(meshCommand.material))
                {
                    blinphong->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
                    mesh->SetWorldTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.5f,0.5f,0.5f)));
                }

                meshCommand.vertices = mesh->GetVertices();
                meshCommand.indices = mesh->GetIndices();
                meshCommand.transform = mesh->GetWorldTransform();
                meshCommand.state = RenderMode::Opaque;
                meshCommand.hasUV = true;
                meshCommand.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;

                commands.push_back(meshCommand);
            }
            
            // 使用RenderPassManager执行Pass（有正确的依赖关系管理）
            te::RenderPassManager::GetInstance().ExecuteAll(commands);
            //g_renderer->ExecuteRenderPasses(commands);
        }
        else
        {
            // 传统单Pass渲染
            //g_renderer->DrawBackgroud();
            for (auto mesh : g_model->GetMeshList())
            {
                g_renderer->DrawMesh(mesh->GetVertices(),
                    mesh->GetIndices(),
                    mesh->GetMaterial(),
                    mesh->GetWorldTransform());
            }
           
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
