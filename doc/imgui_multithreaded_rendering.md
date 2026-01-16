# ImGui 在多线程渲染架构下的处理方案

## 概述

在多线程渲染架构中，ImGui 的处理需要特别注意，因为：
1. ImGui 需要在主线程中处理输入事件
2. ImGui 的渲染需要在 3D 场景渲染完成后进行
3. OpenGL 上下文访问需要线程同步

## 正确的执行流程

### 多线程渲染模式下的 ImGui 流程

```
主线程执行顺序：
┌─────────────────────────────────────────────────────────────┐
│ 1. 处理输入 (Process Input)                                  │
│    - glfwPollEvents() (上一帧)                                │
│    - EventHelper::processInput()                              │
├─────────────────────────────────────────────────────────────┤
│ 2. 准备 ImGui 帧 (Prepare ImGui Frame)                       │
│    - 获取 OpenGL 上下文锁                                     │
│    - ImGui_ImplOpenGL3_NewFrame()                            │
│    - ImGui_ImplGlfw_NewFrame()                                │
│    - ImGui::NewFrame()                                        │
│    - 释放 OpenGL 上下文锁                                     │
│    注意：NewFrame 必须在主线程中调用，用于处理输入             │
├─────────────────────────────────────────────────────────────┤
│ 3. 生成渲染命令 (Generate Render Commands)                    │
│    - 构建场景渲染命令                                         │
│    - 推送到 RenderCommandQueue                               │
├─────────────────────────────────────────────────────────────┤
│ 4. 构建 ImGui UI (Build ImGui UI)                            │
│    - BuildImGuiUI()                                          │
│    - 不需要 OpenGL 上下文                                     │
│    - 可以访问 ImGui API 构建 UI                               │
├─────────────────────────────────────────────────────────────┤
│ 5. 信号帧准备完成 (Signal Frame Ready)                        │
│    - mpFrameSync->SignalFrameReady()                         │
│    - 渲染线程开始执行 3D 渲染                                 │
├─────────────────────────────────────────────────────────────┤
│ 6. 等待渲染完成 (Wait for Render Complete)                    │
│    - mpFrameSync->WaitForRenderComplete()                    │
│    - 3D 场景渲染已完成                                        │
├─────────────────────────────────────────────────────────────┤
│ 7. 渲染 ImGui (Render ImGui)                                 │
│    - 获取 OpenGL 上下文锁                                     │
│    - ImGui::Render()                                         │
│    - ImGui_ImplOpenGL3_RenderDrawData()                      │
│    - glfwSwapBuffers()                                        │
│    - 释放 OpenGL 上下文锁                                     │
│    注意：ImGui 渲染在 3D 场景之上                             │
├─────────────────────────────────────────────────────────────┤
│ 8. 轮询事件 (Poll Events)                                     │
│    - glfwPollEvents()                                         │
│    - 为下一帧准备输入                                          │
└─────────────────────────────────────────────────────────────┘
```

## 关键实现细节

### 1. ImGui 帧准备（NewFrame）

**时机**：必须在帧开始时，在生成渲染命令之前

**原因**：
- `ImGui::NewFrame()` 会处理输入事件
- 需要访问 GLFW 窗口状态
- 必须在主线程中调用

**实现**：
```cpp
// 在生成渲染命令之前
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    glfwMakeContextCurrent(mWindow);
    
    // Start ImGui frame (this processes input and prepares UI state)
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    glfwMakeContextCurrent(nullptr);  // release context
}
```

### 2. ImGui UI 构建（BuildImGuiUI）

**时机**：在 NewFrame 之后，在信号帧准备之前

**原因**：
- 不需要 OpenGL 上下文
- 可以并行处理（虽然当前在主线程）
- 构建 UI 状态和布局

**实现**：
```cpp
void RenderAgent::BuildImGuiUI()
{
    // Build ImGui UI (this can be called without OpenGL context)
    // Note: ImGui::NewFrame() must be called before this
    
    ImGui::Begin("Mouse Picking Demo");
    // ... UI code ...
    ImGui::End();
}
```

### 3. ImGui 渲染（Render）

**时机**：在 3D 场景渲染完成后，在交换缓冲区之前

**原因**：
- ImGui 需要渲染在 3D 场景之上
- 需要 OpenGL 上下文
- 必须在主线程中调用

**实现**：
```cpp
// 等待 3D 渲染完成后
{
    std::lock_guard<std::mutex> lock(g_GLContextMutex);
    glfwMakeContextCurrent(mWindow);
    
    // Render ImGui draw data (this actually draws the UI)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Swap buffers
    glfwSwapBuffers(mWindow);
    
    glfwMakeContextCurrent(nullptr);  // release context
}
```

## 代码结构

### RenderAgent 中的方法分离

```cpp
class RenderAgent
{
private:
    // 单线程渲染模式使用（保持向后兼容）
    void RenderImGui();  // 包含 NewFrame + BuildUI + Render
    
    // 多线程渲染模式使用
    void BuildImGuiUI();  // 只构建 UI，不需要 OpenGL 上下文
};
```

### 多线程渲染循环

```cpp
void RenderAgent::Render()
{
    while (!glfwWindowShouldClose(mWindow))
    {
        // 1. 处理输入
        EventHelper::GetInstance().processInput(mWindow);
        
        if (mMultithreadedRendering)
        {
            // 2. 准备 ImGui 帧（需要 OpenGL 上下文）
            {
                std::lock_guard<std::mutex> lock(g_GLContextMutex);
                glfwMakeContextCurrent(mWindow);
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                glfwMakeContextCurrent(nullptr);
            }
            
            // 3. 生成渲染命令
            // ... generate commands ...
            mpCommandQueue->PushCommands(commands);
            
            // 4. 构建 ImGui UI（不需要 OpenGL 上下文）
            BuildImGuiUI();
            
            // 5. 信号帧准备完成
            mpFrameSync->SignalFrameReady();
            
            // 6. 等待渲染完成
            mpFrameSync->WaitForRenderComplete();
            
            // 7. 渲染 ImGui（需要 OpenGL 上下文）
            {
                std::lock_guard<std::mutex> lock(g_GLContextMutex);
                glfwMakeContextCurrent(mWindow);
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glfwSwapBuffers(mWindow);
                glfwMakeContextCurrent(nullptr);
            }
            
            // 8. 轮询事件
            glfwPollEvents();
        }
        else
        {
            // 单线程模式（使用旧的 RenderImGui 方法）
            // ...
            RenderImGui();
            // ...
        }
    }
}
```

## 线程安全注意事项

### 1. OpenGL 上下文同步

- 所有 OpenGL 调用（包括 ImGui 的 OpenGL 调用）必须在互斥锁保护下进行
- 主线程和渲染线程不能同时访问 OpenGL 上下文

### 2. ImGui API 调用

- `ImGui::NewFrame()` - 必须在主线程中调用
- `ImGui::Begin()` / `ImGui::End()` - 可以在主线程中无锁调用（不需要 OpenGL 上下文）
- `ImGui::Render()` - 必须在主线程中调用，需要 OpenGL 上下文
- `ImGui_ImplOpenGL3_RenderDrawData()` - 必须在主线程中调用，需要 OpenGL 上下文

### 3. GLFW 窗口操作

- `glfwPollEvents()` - 必须在主线程中调用
- `glfwSwapBuffers()` - 必须在主线程中调用
- `glfwMakeContextCurrent()` - 需要互斥锁保护

## 性能考虑

### 1. 上下文切换开销

每次获取/释放 OpenGL 上下文都有开销，因此：
- 尽量减少上下文切换次数
- 将相关的 OpenGL 操作集中在一起

### 2. 锁竞争

- 使用细粒度锁（当前实现使用全局锁，可以优化）
- 避免在锁内执行耗时操作

### 3. 并行化机会

- `BuildImGuiUI()` 可以在渲染线程执行 3D 渲染时并行执行（虽然当前在主线程）
- 未来可以考虑将 UI 构建移到单独的线程

## 常见问题

### Q1: 为什么 NewFrame 要在渲染命令生成之前调用？

**A**: `ImGui::NewFrame()` 会处理输入事件和更新内部状态，这些信息需要在构建 UI 时可用。

### Q2: 为什么不能将 ImGui 渲染放到渲染线程？

**A**: 
- ImGui 的 OpenGL 实现依赖于主线程的上下文
- GLFW 的输入处理必须在主线程
- `glfwSwapBuffers()` 必须在主线程调用

### Q3: 如果渲染线程很慢，ImGui 会卡顿吗？

**A**: 会。因为主线程在等待渲染完成。可以通过以下方式优化：
- 使用异步渲染（不等待渲染完成）
- 使用多缓冲（渲染上一帧，准备下一帧）

### Q4: 如何减少上下文切换开销？

**A**: 
- 创建真正的共享 OpenGL 上下文（需要平台特定 API）
- 使用更细粒度的同步机制
- 考虑使用无锁数据结构

## 最佳实践

1. **分离 UI 构建和渲染**
   - `BuildImGuiUI()` 只构建 UI 状态
   - `Render()` 只执行实际的 OpenGL 渲染

2. **最小化上下文锁定时间**
   - 只在必要时获取锁
   - 尽快释放锁

3. **保持向后兼容**
   - 单线程模式使用 `RenderImGui()`
   - 多线程模式使用分离的方法

4. **错误处理**
   - 检查 OpenGL 上下文是否有效
   - 处理上下文切换失败的情况

## 总结

在多线程渲染架构中，ImGui 的处理需要：
1. **早期准备**：在帧开始时调用 `NewFrame()`
2. **分离构建**：UI 构建不需要 OpenGL 上下文
3. **延迟渲染**：在 3D 渲染完成后渲染 ImGui
4. **线程同步**：使用互斥锁保护 OpenGL 上下文访问

这种设计既保证了线程安全，又最大化了并行化机会。

---

*文档版本：1.0*  
*最后更新：2024*
