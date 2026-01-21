#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "framework/Renderer.h"

class RenderCommandQueue
{
public:
    RenderCommandQueue() = default;
    ~RenderCommandQueue() = default;
    
    // unallow copy
    RenderCommandQueue(const RenderCommandQueue&) = delete;
    RenderCommandQueue& operator=(const RenderCommandQueue&) = delete;
    
    // push single command
    void PushCommand(const RenderCommand& command);
    
    // push batch commands
    void PushCommands(const std::vector<RenderCommand>& commands);
    
    // pop command (non-blocking)
    std::optional<RenderCommand> PopCommand();
    
    // wait and pop command (blocking)
    std::optional<RenderCommand> WaitAndPopCommand();
    
    // clear queue
    void Clear();
    
    // get queue size
    size_t Size() const;
    
    // is empty
    bool Empty() const;

private:
    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    std::queue<RenderCommand> mCommands;
};