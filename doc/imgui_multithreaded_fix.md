# ImGui 多线程渲染问题修复

## 问题描述

在多线程渲染模式下，ImGui UI 没有显示出来，只有场景模型可见。

## 根本原因

1. **OpenGL 上下文未正确释放**：渲染线程在完成 3D 场景渲染后，没有释放 OpenGL 上下文，导致主线程无法正确获取上下文来渲染 ImGui。

2. **上下文状态不一致**：当主线程获取上下文时，OpenGL 状态仍然是渲染线程设置的状态（深度测试启用、混合可能未启用等），虽然 ImGui 会设置自己的状态，但上下文切换可能导致状态不一致。

## 解决方案

### 1. 渲染线程在渲染完成后释放上下文

**修改文件**：`source/framework/RenderThread.cpp`

```cpp
// 在 EndFrame() 之后，SignalRenderComplete() 之前
mpRenderer->EndFrame();

// IMPORTANT: Release context after rendering so main thread can use it for ImGui
// The context will be re-bound in the next frame
glfwMakeContextCurrent(nullptr);
```

**原因**：
- 确保主线程可以获取上下文
- 避免上下文状态冲突

### 2. 渲染线程在下一帧开始时重新绑定上下文

**修改文件**：`source/framework/RenderThread.cpp`

```cpp
// 在渲染循环开始时
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    
    // IMPORTANT: Re-bind context at the start of each frame
    // The context was released after the previous frame's ImGui rendering
    glfwMakeContextCurrent(mMainWindow);
    
    // ... 开始渲染 ...
}
```

**原因**：
- 确保渲染线程在每帧开始时都有正确的上下文
- 上下文在上一帧被主线程用于 ImGui 渲染后需要重新绑定

### 3. 主线程正确获取和释放上下文

**修改文件**：`TinyRenderer/source/RenderAgent.cpp`

```cpp
// 在等待渲染完成后
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    glfwMakeContextCurrent(mWindow);
    
    // Render ImGui
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0)
    {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }
    
    // Swap buffers
    glfwSwapBuffers(mWindow);
    
    // Release context so render thread can use it in the next frame
    glfwMakeContextCurrent(nullptr);
}
```

**原因**：
- 确保主线程在正确的时机获取上下文
- 渲染完成后立即释放，让渲染线程可以在下一帧使用

## 执行流程（修复后）

```
帧 N 执行流程：
┌─────────────────────────────────────────────────────────────┐
│ 主线程                                                         │
│ 1. NewFrame() (获取上下文)                                    │
│ 2. 生成渲染命令                                                │
│ 3. BuildImGuiUI()                                            │
│ 4. 信号帧准备完成                                              │
│ 5. 等待渲染完成                                                │
│ 6. 获取上下文                                                  │
│ 7. Render ImGui                                               │
│ 8. SwapBuffers                                                │
│ 9. 释放上下文                                                  │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ 信号
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 渲染线程                                                       │
│ 1. 等待帧准备                                                  │
│ 2. 获取上下文 (重新绑定)                                       │
│ 3. 渲染 3D 场景                                                │
│ 4. EndFrame()                                                 │
│ 5. 释放上下文 ← 关键修复点                                     │
│ 6. 信号渲染完成                                                │
└─────────────────────────────────────────────────────────────┘
```

## 关键修复点

### 修复前的问题

```cpp
// 渲染线程
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    glfwMakeContextCurrent(mMainWindow);
    // ... 渲染 ...
    mpRenderer->EndFrame();
    // 上下文仍然绑定！主线程无法正确获取
}
mFrameSync->SignalRenderComplete();
```

### 修复后的正确实现

```cpp
// 渲染线程
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    glfwMakeContextCurrent(mMainWindow);
    // ... 渲染 ...
    mpRenderer->EndFrame();
    glfwMakeContextCurrent(nullptr);  // ← 关键：释放上下文
}
mFrameSync->SignalRenderComplete();
```

## 验证方法

1. **检查 ImGui 是否显示**：
   - 运行程序，应该能看到 ImGui 窗口
   - 检查控制台是否有错误信息

2. **检查上下文切换**：
   - 添加日志输出，确认上下文正确切换
   - 确认没有上下文相关的 OpenGL 错误

3. **检查渲染顺序**：
   - 3D 场景应该先渲染
   - ImGui 应该渲染在 3D 场景之上
   - 缓冲区交换应该在 ImGui 渲染之后

## 注意事项

1. **上下文生命周期**：
   - 上下文必须在正确的线程中绑定和释放
   - 不能同时有两个线程持有同一个上下文

2. **状态管理**：
   - ImGui 会自动设置和恢复 OpenGL 状态
   - 但确保上下文切换时状态一致

3. **性能考虑**：
   - 上下文切换有开销
   - 尽量减少切换次数
   - 考虑使用真正的共享上下文（需要平台特定 API）

## 相关文件

- `source/framework/RenderThread.cpp` - 渲染线程实现
- `TinyRenderer/source/RenderAgent.cpp` - 主线程渲染逻辑
- `doc/imgui_multithreaded_rendering.md` - ImGui 多线程渲染文档

---
