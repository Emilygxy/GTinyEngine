#include "framework/RenderCommandQueue.h"
#include <iostream>

void RenderCommandQueue::PushCommand(const RenderCommand& command)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mCommands.push(command);
    mCondition.notify_one();
}

void RenderCommandQueue::PushCommands(const std::vector<RenderCommand>& commands)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto& cmd : commands)
    {
        mCommands.push(cmd);
    }
    mCondition.notify_one();
}

std::optional<RenderCommand> RenderCommandQueue::PopCommand()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCommands.empty())
    {
        return std::nullopt;
    }
    
    RenderCommand cmd = mCommands.front();
    mCommands.pop();
    return cmd;
}

std::optional<RenderCommand> RenderCommandQueue::WaitAndPopCommand()
{
    std::unique_lock<std::mutex> lock(mMutex);
    mCondition.wait(lock, [this] { return !mCommands.empty(); });
    
    if (mCommands.empty())
    {
        return std::nullopt;
    }
    
    RenderCommand cmd = mCommands.front();
    mCommands.pop();
    return cmd;
}

void RenderCommandQueue::Clear()
{
    std::lock_guard<std::mutex> lock(mMutex);
    while (!mCommands.empty())
    {
        mCommands.pop();
    }
}

size_t RenderCommandQueue::Size() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mCommands.size();
}

bool RenderCommandQueue::Empty() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mCommands.empty();
}
