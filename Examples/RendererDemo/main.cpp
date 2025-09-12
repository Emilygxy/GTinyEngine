#include <iostream>
#include <memory>
#include "framework/Renderer.h"
#include "materials/BlinnPhongMaterial.h"
#include "geometry/Sphere.h"
#include "geometry/Torus.h"
#include "Camera.h"
#include "Light.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

class RendererDemo
{
public:
    RendererDemo() = default;
    ~RendererDemo() = default;

    bool Initialize()
    {
        // 初始化 GLFW
        if (!glfwInit())
        {
            std::cout << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // 创建窗口
        mWindow = glfwCreateWindow(800, 600, "Renderer Demo", nullptr, nullptr);
        if (!mWindow)
        {
            std::cout << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(mWindow);

        // 初始化 GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cout << "Failed to initialize GLAD" << std::endl;
            return false;
        }

        // 创建渲染器
        mpRenderer = RendererFactory::CreateRenderer(RendererBackend::OpenGL);
        if (!mpRenderer || !mpRenderer->Initialize())
        {
            std::cout << "Failed to initialize renderer" << std::endl;
            return false;
        }

        // 设置相机
        mpCamera = std::make_shared<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
        mpCamera->SetAspectRatio(800.0f / 600.0f);
        mpRenderer->SetCamera(mpCamera);

        // 设置光照
        mpLight = std::make_shared<Light>();
        mpLight->SetPosition(glm::vec3(2.0f, 2.0f, 2.0f));
        mpLight->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
        mpRenderer->SetLight(mpLight);

        // 创建几何体
        mpSphere = std::make_shared<Sphere>(1.0f, 32, 32);
        mpTorus = std::make_shared<Torus>(1.0f, 0.3f, 32, 32);

        // 创建材质
        mpBlinnPhongMaterial = std::make_shared<BlinnPhongMaterial>();
        mpBlinnPhongMaterial->AttachedCamera(mpCamera);
        mpBlinnPhongMaterial->AttachedLight(mpLight);

        return true;
    }

    void Run()
    {
        while (!glfwWindowShouldClose(mWindow))
        {
            // 处理输入
            ProcessInput();

            // 开始渲染帧
            mpRenderer->BeginFrame();

            // 设置视口和清除颜色
            mpRenderer->SetViewport(0, 0, 800, 600);
            mpRenderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            mpRenderer->Clear(0x3); // 清除颜色和深度缓冲

            // 渲染球体
            RenderSphere();

            // 渲染环面
            RenderTorus();

            // 结束渲染帧
            mpRenderer->EndFrame();

            // 显示渲染统计
            DisplayStats();

            // 交换缓冲区和处理事件
            glfwSwapBuffers(mWindow);
            glfwPollEvents();
        }
    }

    void Shutdown()
    {
        mpRenderer->Shutdown();
        glfwTerminate();
    }

private:
    void ProcessInput()
    {
        if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(mWindow, true);

        // 切换 Blinn-Phong 算法
        static bool lastKeyState = false;
        bool currentKeyState = glfwGetKey(mWindow, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (currentKeyState && !lastKeyState)
        {
            mUseBlinnPhong = !mUseBlinnPhong;
            mpBlinnPhongMaterial->SetUseBlinnPhong(mUseBlinnPhong);
            std::cout << "Switched to " << (mUseBlinnPhong ? "Blinn-Phong" : "Phong") << " shading" << std::endl;
        }
        lastKeyState = currentKeyState;
    }

    void RenderSphere()
    {
        // 使用统一的 DrawMesh 接口渲染球体
        mpRenderer->DrawMesh(mpSphere->GetVertices(), 
                           mpSphere->GetIndices(), 
                           mpBlinnPhongMaterial,
                           glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 0.0f, 0.0f)));
    }

    void RenderTorus()
    {
        // 使用统一的 DrawMesh 接口渲染环面
        mpRenderer->DrawMesh(mpTorus->GetVertices(), 
                           mpTorus->GetIndices(), 
                           mpBlinnPhongMaterial,
                           glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.0f, 0.0f)));
    }

    void DisplayStats()
    {
        const auto& stats = mpRenderer->GetRenderStats();
        std::cout << "\rDraw Calls: " << stats.drawCalls 
                  << " | Triangles: " << stats.triangles 
                  << " | Vertices: " << stats.vertices << std::flush;
    }

private:
    GLFWwindow* mWindow = nullptr;
    std::unique_ptr<IRenderer> mpRenderer;
    std::shared_ptr<Camera> mpCamera;
    std::shared_ptr<Light> mpLight;
    std::shared_ptr<Sphere> mpSphere;
    std::shared_ptr<Torus> mpTorus;
    std::shared_ptr<BlinnPhongMaterial> mpBlinnPhongMaterial;
    bool mUseBlinnPhong = true;
};

int main()
{
    RendererDemo demo;
    
    if (!demo.Initialize())
    {
        std::cout << "Failed to initialize demo" << std::endl;
        return -1;
    }

    std::cout << "Renderer Demo Started" << std::endl;
    std::cout << "Press SPACE to toggle between Blinn-Phong and Phong shading" << std::endl;
    std::cout << "Press ESC to exit" << std::endl;

    demo.Run();
    demo.Shutdown();

    return 0;
} 