#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

class FrameSync
{
public:
    FrameSync() = default;
    ~FrameSync() = default;
    
    // 禁止拷贝
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;
    
    // 主线程：标记帧准备完成
    void SignalFrameReady();
    
    // 渲染线程：等待帧准备
    void WaitForFrameReady();
    
    // 渲染线程：标记渲染完成
    void SignalRenderComplete();
    
    // 主线程：等待渲染完成
    void WaitForRenderComplete();
    
    // 非阻塞检查
    bool IsFrameReady() const;
    bool IsRenderComplete() const;

private:
    std::atomic<bool> mFrameReady{false};
    std::atomic<bool> mRenderComplete{false};
    mutable std::mutex mMutex;
    std::condition_variable mFrameReadyCondition;
    std::condition_variable mRenderCompleteCondition;
};
