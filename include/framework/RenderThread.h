#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include "framework/RenderCommandQueue.h"
#include "framework/FrameSync.h"
#include "framework/Renderer.h"

struct GLFWwindow;

// global mutex for synchronizing OpenGL context access
// note: this is a simplified implementation, in actual application, should use more fine-grained synchronization mechanism
extern std::mutex g_GLContextMutex;

class RenderThread
{
public:
    RenderThread(
        std::shared_ptr<RenderCommandQueue> queue,
        std::shared_ptr<FrameSync> sync,
        std::shared_ptr<IRenderer> renderer,
        GLFWwindow* window = nullptr
    );
    
    ~RenderThread();
    
    // start render thread
    bool Start();
    
    // stop render thread
    void Stop();
    
    // wait thread end
    void Join();
    
    // check if running
    bool IsRunning() const { return mRunning.load(); }
    
    // set main window (must be called before Start)
    void SetMainWindow(GLFWwindow* window) { mMainWindow = window; }
    
    // set render view for viewport size
    void SetRenderView(std::shared_ptr<class RenderView> view) { mpRenderView = view; }

private:
    void RenderLoop();
    void InitializeRenderContext();
    void CleanupRenderContext();
    
    std::shared_ptr<RenderCommandQueue> mCommandQueue;
    std::shared_ptr<FrameSync> mFrameSync;
    std::shared_ptr<IRenderer> mpRenderer;
    
    std::thread mThread;
    std::atomic<bool> mRunning{false};
    
    // OpenGL context related
    GLFWwindow* mMainWindow = nullptr;  // main window pointer
    void* mRenderContext = nullptr;  // maybe different types depending on the platform
    std::shared_ptr<class RenderView> mpRenderView = nullptr;  // for getting viewport size
};