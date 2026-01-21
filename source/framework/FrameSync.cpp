#include "framework/FrameSync.h"
#include <iostream>

void FrameSync::SignalFrameReady()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFrameReady = true;
    }
    mFrameReadyCondition.notify_one();
}

void FrameSync::WaitForFrameReady()
{
    std::unique_lock<std::mutex> lock(mMutex);
    mFrameReadyCondition.wait(lock, [this] { return mFrameReady.load(); });
    mFrameReady = false;
}

void FrameSync::SignalRenderComplete()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRenderComplete = true;
    }
    mRenderCompleteCondition.notify_one();
}

void FrameSync::WaitForRenderComplete()
{
    std::unique_lock<std::mutex> lock(mMutex);
    mRenderCompleteCondition.wait(lock, [this] { return mRenderComplete.load(); });
    mRenderComplete = false;
}

bool FrameSync::IsFrameReady() const
{
    return mFrameReady.load();
}

bool FrameSync::IsRenderComplete() const
{
    return mRenderComplete.load();
}
