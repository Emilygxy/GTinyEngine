#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

class FrameSync
{
public:
    FrameSync() = default;
    ~FrameSync() = default;
    
    //  disable copy
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;
    
    //  main thread: signal frame ready
    void SignalFrameReady();
    
    //  render thread: wait for frame ready
    void WaitForFrameReady();
    
    //  render thread: signal render complete
    void SignalRenderComplete();
    
    //  main thread: wait for render complete
    void WaitForRenderComplete();
    
    //  non-blocking check
    bool IsFrameReady() const;
    bool IsRenderComplete() const;

private:
    std::atomic<bool> mFrameReady{false};
    std::atomic<bool> mRenderComplete{false};
    mutable std::mutex mMutex;
    std::condition_variable mFrameReadyCondition;
    std::condition_variable mRenderCompleteCondition;
};
