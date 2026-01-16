#include "framework/RenderThread.h"
#include "framework/RenderPass.h"
#include "RenderView.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <iostream>
#include <chrono>
#include <mutex>

// global mutex for synchronizing OpenGL context access
std::mutex g_GLContextMutex;

RenderThread::RenderThread(
    std::shared_ptr<RenderCommandQueue> queue,
    std::shared_ptr<FrameSync> sync,
    std::shared_ptr<IRenderer> renderer,
    GLFWwindow* window
) : mCommandQueue(queue)
  , mFrameSync(sync)
  , mpRenderer(renderer)
  , mRunning(false)
  , mMainWindow(window)
  , mRenderContext(nullptr)
{
}

RenderThread::~RenderThread()
{
    Stop();
    Join();
}

bool RenderThread::Start()
{
    if (mRunning.load())
    {
        std::cout << "RenderThread::Start - Thread already running" << std::endl;
        return false;
    }
    
    mRunning = true;
    mThread = std::thread(&RenderThread::RenderLoop, this);
    
    std::cout << "RenderThread::Start - Render thread started" << std::endl;
    return true;
}

void RenderThread::Stop()
{
    if (!mRunning.load())
    {
        return;
    }
    
    mRunning = false;
    
    // wake up waiting thread
    if (mFrameSync)
    {
        mFrameSync->SignalFrameReady();
    }
    
    std::cout << "RenderThread::Stop - Stopping render thread" << std::endl;
}

void RenderThread::Join()
{
    if (mThread.joinable())
    {
        mThread.join();
        std::cout << "RenderThread::Join - Render thread joined" << std::endl;
    }
}

void RenderThread::RenderLoop()
{
    std::cout << "RenderThread::RenderLoop - Entering render loop" << std::endl;
    
    // initialize render context
    // note: due to GLFW's limitation, we need to use main window
    // in actual application, should create shared context, but here for simplicity, we use main window
    // this needs to ensure main thread does not use OpenGL context at the same time
    
    // get main window
    if (!mMainWindow)
    {
        std::cerr << "RenderThread::RenderLoop - Main window not set!" << std::endl;
        mRunning = false;
        return;
    }
    
    // bind context in render thread
    // use mutex to ensure no conflict with main thread
    {
        std::lock_guard<std::mutex> lock(g_GLContextMutex);
        glfwMakeContextCurrent(mMainWindow);
    }
    
    // ensure GLAD is loaded (if not already)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "RenderThread::RenderLoop - Failed to initialize GLAD" << std::endl;
        mRunning = false;
        return;
    }
    
    std::cout << "RenderThread::RenderLoop - OpenGL context initialized" << std::endl;
    
    // set render state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // render loop
    while (mRunning.load())
    {
        // wait for frame ready signal
        mFrameSync->WaitForFrameReady();
        
        if (!mRunning.load())
        {
            break;
        }
        
        // collect render commands
        std::vector<RenderCommand> commands;
        commands.reserve(1024);  // pre-allocate space
        
        // batch collect commands
        while (auto cmd = mCommandQueue->PopCommand())
        {
            commands.push_back(*cmd);
            
            // limit batch size, avoid processing too many at once
            if (commands.size() >= 1024)
            {
                break;
            }
        }
        
        if (commands.empty())
        {
            // if no commands, continue waiting for next frame
            mFrameSync->SignalRenderComplete();
            continue;
        }
        
        // start rendering frame (under mutex protection)
        {
            std::lock_guard<std::mutex> lock(g_GLContextMutex);
            
            // ensure context is bound
            glfwMakeContextCurrent(mMainWindow);
            
            // start rendering frame
            mpRenderer->BeginFrame();
            
            // set viewport and clear color (these should be set in BeginFrame, but for safety, we set them here)
            // get viewport size from RenderView
            uint16_t viewportWidth = 800;
            uint16_t viewportHeight = 600;
            if (mpRenderView)
            {
                viewportWidth = mpRenderView->Width();
                viewportHeight = mpRenderView->Height();
            }
            mpRenderer->SetViewport(0, 0, viewportWidth, viewportHeight);
            mpRenderer->SetClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            mpRenderer->Clear(0x3);  // clear color and depth
            
            // execute rendering
            if (mpRenderer->IsMultiPassEnabled())
            {
                // multi-pass rendering
                te::RenderPassManager::GetInstance().ExecuteAll(commands);
            }
            else
            {
                // single-pass rendering
                for (const auto& cmd : commands)
                {
                    mpRenderer->DrawMesh(cmd);
                }
            }
            
            // end rendering frame
            mpRenderer->EndFrame();
        }
        
        // notify main thread rendering complete
        mFrameSync->SignalRenderComplete();
    }
    
    // cleanup: release context
    {
        std::lock_guard<std::mutex> lock(g_GLContextMutex);
        glfwMakeContextCurrent(nullptr);
    }
    
    std::cout << "RenderThread::RenderLoop - Exiting render loop" << std::endl;
}

void RenderThread::InitializeRenderContext()
{
    // context initialization in RenderLoop
    // this method can be used for other initialization work
}

void RenderThread::CleanupRenderContext()
{
    // cleanup work in RenderLoop end
    // this method can be used for other cleanup work
}
