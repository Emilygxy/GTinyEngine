#include <iostream>
#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "glm/fwd.hpp"
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

// Video system includes
#include "video/VideoPlayer.h"
#include "video/VideoMaterial.h"
#include "video/VideoGeometry.h"

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

// Video system
std::shared_ptr<te::VideoPlayer> g_videoPlayer = nullptr;
std::shared_ptr<te::VideoMaterial> g_videoMaterial = nullptr;
std::shared_ptr<te::VideoGeometry> g_videoPlane = nullptr;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    if (g_RenderView) {
        g_RenderView->ResizeViewport(width, height);
    }
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // video control
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (g_videoPlayer) {
            if (g_videoPlayer->IsPlaying()) {
                g_videoPlayer->Pause();
                std::cout << "Video paused" << std::endl;
            } else {
                g_videoPlayer->Play();
                std::cout << "Video playing" << std::endl;
            }
        }
    }
    
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (g_videoPlayer) {
            g_videoPlayer->Stop();
            g_videoPlayer->Play();
            std::cout << "Video restarted" << std::endl;
        }
    }
    
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        if (g_videoPlayer) {
            g_videoPlayer->Seek(5.0); // 跳转到5秒
            std::cout << "Video seeked to 5 seconds" << std::endl;
        }
    }
}

void setupMultiPassRendering()
{
    // create skybox pass
    auto skyboxPass = std::make_shared<te::SkyboxPass>();
    skyboxPass->Initialize(g_RenderView, g_RenderContext);

    // create geometry pass 
    auto geometryPass = std::make_shared<te::GeometryPass>();
    geometryPass->Initialize(g_RenderView, g_RenderContext);

    // create light pass
    auto basePass = std::make_shared<te::BasePass>();
    basePass->Initialize(g_RenderView, g_RenderContext);

    // PostProcessPass Pass
    auto postProcessPass = std::make_shared<te::PostProcessPass>();
    postProcessPass->Initialize(g_RenderView, g_RenderContext);
    postProcessPass->AddEffect("Blit", std::make_shared<BlitMaterial>());

    // add pass to RenderPassManager (for dependency management)
    te::RenderPassManager::GetInstance().AddPass(skyboxPass);
    te::RenderPassManager::GetInstance().AddPass(geometryPass);
    te::RenderPassManager::GetInstance().AddPass(basePass);
    te::RenderPassManager::GetInstance().AddPass(postProcessPass);

    // 启用多Pass渲染
    g_renderer->SetMultiPassEnabled(true);
}

void setupVideoSystem()
{
    std::cout << "Setting up video system..." << std::endl;
    
    // 创建视频播放器
    g_videoPlayer = std::make_shared<te::VideoPlayer>();
    
    // try to load video file
    std::vector<std::string> testVideos = {
        "resources/videos/test_video.mp4",
        //"resources/videos/sample.avi",
        //"resources/videos/demo.mov"
    };
    
    bool videoLoaded = false;
    for (const auto& videoPath : testVideos) {
        if (g_videoPlayer->LoadVideo(videoPath)) {
            videoLoaded = true;
            std::cout << "Successfully loaded video: " << videoPath << std::endl;
            break;
        }
    }
    
    if (!videoLoaded) {
        std::cout << "No video files found, using simulated video" << std::endl;
        // even if there is no real video file, VideoPlayer will create a simulated video
        g_videoPlayer->LoadVideo("resources/videos/nonexistent.mp4");
    }
    
    // create video material
    g_videoMaterial = std::make_shared<te::VideoMaterial>();
    g_videoMaterial->SetVideoPlayer(g_videoPlayer);
    
    // create video plane geometry
    g_videoPlane = std::make_shared<te::VideoGeometry>(4.0f, 3.0f); // 4:3 宽高比
    g_videoPlane->SetVideoMaterial(g_videoMaterial);
    
    // set video plane position
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, glm::vec3(0.0f, 0.0f, -3.0f));
    trans = glm::rotate(trans, glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    g_videoPlane->SetWorldTransform(trans);
    
    // start playing video
    g_videoPlayer->Play();
    
    std::cout << "Video system setup completed" << std::endl;
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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Video Render Demo with FFmpeg", NULL, NULL);
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

    // create renderer
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

    // create RenderView and RenderContext
    g_RenderView = std::make_shared<RenderView>(SCR_WIDTH, SCR_HEIGHT);
    g_RenderContext = std::make_shared<RenderContext>();
    g_renderer->SetRenderContext(g_RenderContext);

    // create camera
    g_camera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 8.0f));
    g_camera->SetAspectRatio((float)SCR_WIDTH / (float)SCR_HEIGHT);
    g_RenderContext->AttachCamera(g_camera);

    // create light
    g_light = std::make_shared<Light>();
    g_light->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
    g_light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    g_RenderContext->PushAttachLight(g_light);

    // create geometry
    g_sphere = std::make_shared<Sphere>();
    auto material = std::make_shared<BlinnPhongMaterial>();
    material->SetDiffuseTexturePath("resources/textures/IMG_8515.JPG");
    g_sphere->SetMaterial(material);

    // set multi pass rendering
    setupMultiPassRendering();
    
    // set video system
    setupVideoSystem();

    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "SPACE: Play/Pause video" << std::endl;
    std::cout << "R: Restart video" << std::endl;
    std::cout << "S: Seek to 5 seconds" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "================\n" << std::endl;

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // input
        processInput(window);

        // Begin Render Frame
        g_renderer->BeginFrame();

        g_renderer->SetViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        g_renderer->SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        g_renderer->Clear(0x3);

        // execute multi pass rendering
        if (g_renderer->IsMultiPassEnabled())
        {
            // create render command
            std::vector<RenderCommand> commands;
            
            // add sphere command
            RenderCommand sphereCommand;
            sphereCommand.material = g_sphere->GetMaterial();
            sphereCommand.vertices = g_sphere->GetVertices();
            sphereCommand.indices = g_sphere->GetIndices();
            sphereCommand.transform = g_sphere->GetWorldTransform();
            sphereCommand.state = RenderMode::Opaque;
            sphereCommand.hasUV = true;
            sphereCommand.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;
            commands.push_back(sphereCommand);
            
            // add video plane command
            if (g_videoPlane) {
                RenderCommand videoCommand;
                videoCommand.material = g_videoPlane->GetVideoMaterial();
                videoCommand.vertices = g_videoPlane->GetVertices();
                videoCommand.indices = g_videoPlane->GetIndices();
                videoCommand.transform = g_videoPlane->GetWorldTransform();
                videoCommand.state = RenderMode::Opaque;
                videoCommand.hasUV = true;
                videoCommand.renderpassflag = RenderPassFlag::BaseColor | RenderPassFlag::Geometry;
                commands.push_back(videoCommand);
            }
            
            // use RenderPassManager to execute Pass (with correct dependency management)
            te::RenderPassManager::GetInstance().ExecuteAll(commands);
        }
        else
        {
            // traditional single pass rendering
            g_renderer->DrawMesh(g_sphere->GetVertices(),
                               g_sphere->GetIndices(),
                               g_sphere->GetMaterial(),
                               g_sphere->GetWorldTransform());
                               
            if (g_videoPlane) {
                g_renderer->DrawMesh(g_videoPlane->GetVertices(),
                                   g_videoPlane->GetIndices(),
                                   g_videoPlane->GetVideoMaterial(),
                                   g_videoPlane->GetWorldTransform());
            }
        }

        // End Render Frame
        g_renderer->EndFrame();

        // glfw: swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    if (g_videoPlayer) {
        g_videoPlayer->Stop();
    }
    
    g_renderer->Shutdown();
    glfwTerminate();
    return 0;
}